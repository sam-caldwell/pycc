/***
 * Name: pycc::rt (Runtime impl)
 * Purpose: Minimal precise mark-sweep GC and string objects.
 */
#include "runtime/Runtime.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#ifdef PYCC_WITH_ICU
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#endif
#include <pthread.h>
#include <thread>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pycc::rt {

// GC and runtime tuning constants
static constexpr std::size_t kDefaultListCapacity = 4;
static constexpr uint64_t kDefaultThresholdBytes = 1ULL << 20U;
static constexpr std::size_t kStackSliceWords = 1024;
static constexpr uint64_t kMinLockHoldNs = 2000;
static constexpr uint64_t kSliceIncrementUs = 100;
static constexpr uint64_t kMaxSliceUs = 5000;
static constexpr std::size_t kBatchIncrement = 32;
static constexpr std::size_t kMaxBatch = 512;
static constexpr double kHighPressure = 0.8;
static constexpr double kHighAllocRateBytesPerMs = 4.0;
static constexpr double kLowPressure = 0.3;
static constexpr double kLowAllocRateBytesPerMs = 0.5;
static constexpr uint64_t kSliceLowerTriggerUs = 150;
static constexpr uint64_t kSliceDecrementUs = 50;
static constexpr uint64_t kSliceDefaultUs = 100;
static constexpr std::size_t kBatchLowerTrigger = 64;
static constexpr std::size_t kBatchDecrement = 16;
static constexpr std::size_t kBatchDefault = 32;

struct ObjectHeader {
  uint32_t mark{0};
  uint32_t tag{0};
  std::size_t size{0}; // total allocation size including header
  uint8_t gen{0};      // 0 = young, 1 = old
  uint8_t age{0};      // survival count in young gen
  uint16_t pad{0};
  ObjectHeader* next{nullptr};
};

struct StringPayload { std::size_t len{}; /* char data[] follows */ };
struct BytesPayload  { std::size_t len{}; /* uint8_t data[] follows */ };
struct ByteArrayPayload { std::size_t len{}; std::size_t cap{}; /* uint8_t data[] follows */ };

static std::mutex g_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static ObjectHeader* g_head = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<void**> g_roots; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::size_t g_threshold = kDefaultThresholdBytes; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool g_conservative = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static RuntimeStats g_stats; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<bool> g_bg_enabled{true}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<bool> g_bg_requested{false}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::condition_variable g_bg_cv; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::mutex g_bg_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::thread g_bg_thread; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool g_bg_started = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// Synchronous collection coordination (for gc_collect when background is enabled)
static std::mutex g_gc_done_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::condition_variable g_gc_done_cv; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<uint64_t> g_gc_completed_count{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Reserved for future phase tracking in background GC
enum class GCPhase { Idle, Mark, Sweep };
static ObjectHeader* g_sweep_cur = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static ObjectHeader* g_sweep_prev = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::uintptr_t* g_stack_scan_cur = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::uintptr_t* g_stack_scan_end = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::mutex g_rem_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<void*> g_remembered; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<uint64_t> g_slice_us{kSliceDefaultUs}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<std::size_t> g_sweep_batch{kBatchDefault}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<uint64_t> g_last_bytes_alloc{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<uint64_t> g_last_time_ms{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static double g_ewma_alloc_rate = 0.0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables) bytes/ms
static double g_ewma_pressure = 0.0;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<int> g_barrier_mode{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables) 0=incremental-update, 1=SATB
static bool g_debug = (std::getenv("PYCC_RT_DEBUG") != nullptr); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Segregated free lists for small object sizes (total bytes including header)
static constexpr std::size_t kClassSizes[] = { 64, 128, 256, 512, 1024, 2048, 4096 };
static constexpr int kNumClasses = static_cast<int>(sizeof(kClassSizes) / sizeof(kClassSizes[0]));
static std::vector<ObjectHeader*> g_free_lists[kNumClasses]; // NOLINT
// Thread-local caches for segregated size classes. Mutators steal batches from global lists.
static thread_local std::vector<ObjectHeader*> t_free_lists[kNumClasses]; // NOLINT
static constexpr std::size_t kStealBatch = 16;

static inline int class_index_for(std::size_t total) {
  for (int i = 0; i < kNumClasses; ++i) { if (total <= kClassSizes[i]) return i; }
  return -1;
}

// Thread-local exception state
static thread_local void* t_last_exception = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static thread_local bool t_exc_root_registered = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Forward decl for adaptive controller
static void adapt_controller();
static void start_bg_thread_if_needed();

// Request background GC if pressure is above the threshold.
// Requires caller to hold g_mu; avoids synchronous collection during allocation
// to prevent collecting newly allocated yet-unrooted objects (UAF risk).
static inline void maybe_request_bg_gc_unlocked() {
  if (g_stats.bytesLive <= g_threshold) { return; }
  if (!g_bg_enabled.load(std::memory_order_relaxed)) { return; }
  if (!g_bg_started) { start_bg_thread_if_needed(); }
  g_bg_requested.store(true, std::memory_order_relaxed);
  const std::lock_guard<std::mutex> nlk(g_bg_mu);
  g_bg_cv.notify_one();
}

static void* alloc_raw(std::size_t size, TypeTag tag) {
  // allocate size bytes for payload plus header
  const std::size_t total = sizeof(ObjectHeader) + size;
  unsigned char* mem = nullptr;
  // Try segregated free list first (callers generally hold g_mu)
  int ci = class_index_for(total);
  if (ci >= 0) {
    // Prefer thread-local cache
    if (!t_free_lists[ci].empty()) {
      ObjectHeader* h = t_free_lists[ci].back(); t_free_lists[ci].pop_back();
      mem = reinterpret_cast<unsigned char*>(h);
    } else if (!g_free_lists[ci].empty()) {
      // Steal a small batch from global to seed thread-local cache
      const std::size_t toSteal = std::min<std::size_t>(kStealBatch, g_free_lists[ci].size());
      for (std::size_t i = 0; i < toSteal; ++i) {
        ObjectHeader* h = g_free_lists[ci].back(); g_free_lists[ci].pop_back();
        if (i + 1U < toSteal) { t_free_lists[ci].push_back(h); } else { mem = reinterpret_cast<unsigned char*>(h); }
      }
    }
  }
  if (mem == nullptr) { mem = static_cast<unsigned char*>(::operator new(total)); }
  auto* header = reinterpret_cast<ObjectHeader*>(mem); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  header->mark = 0;
  header->tag = static_cast<uint32_t>(tag);
  header->size = total;
  header->gen = 0; header->age = 0; header->pad = 0;
  header->next = g_head;
  g_head = header;
  g_stats.numAllocated++;
  g_stats.bytesAllocated += total;
  g_stats.bytesLive += total;
  g_stats.peakBytesLive = std::max(g_stats.peakBytesLive, g_stats.bytesLive);
  return mem + sizeof(ObjectHeader); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

static void free_obj(ObjectHeader* header) {
  g_stats.numFreed++;
  g_stats.bytesLive -= header->size;
  if (g_debug) { std::fprintf(stderr, "[runtime] free_obj tag=%u size=%zu\n", header->tag, header->size); }
  int ci = class_index_for(header->size);
  if (ci >= 0) {
    // Sweeper runs on background thread; keep pushing to global list. Mutators will steal into local caches.
    g_free_lists[ci].push_back(header);
  } else {
    ::operator delete(header);
  }
}

// Forward declaration for interior marking
static ObjectHeader* find_object_for_pointer(const void* ptr);

// Convenience wrapper returning stack bounds as an optional pair
// (removed obsolete wrapper)

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
static void mark(ObjectHeader* header) {
  if (header == nullptr || header->mark != 0U) { return; }
  header->mark = 1;
  // Recurse into interior pointers for aggregate types
  switch (static_cast<TypeTag>(header->tag)) {
    case TypeTag::String:
    case TypeTag::Bytes:
    case TypeTag::ByteArray:
    case TypeTag::Int:
    case TypeTag::Float:
    case TypeTag::Bool:
      break; // no interior pointers
    case TypeTag::List: {
      auto* base = reinterpret_cast<unsigned char*>(header); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast)
      auto* payload = reinterpret_cast<std::size_t*>(base + sizeof(ObjectHeader)); // len, cap, then items[]
      const std::size_t len = payload[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast)
      auto* const* items = reinterpret_cast<void* const*>(payload + 2);
      for (std::size_t i = 0; i < len; ++i) {
        const void* valuePtr = items[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (valuePtr == nullptr) { continue; }
        if (ObjectHeader* headerPtr = find_object_for_pointer(valuePtr)) { mark(headerPtr); }
      }
      break;
    }
    case TypeTag::Object: {
      auto* base = reinterpret_cast<unsigned char*>(header); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast)
      auto* payload = reinterpret_cast<std::size_t*>(base + sizeof(ObjectHeader)); // fields, then values[]
      const std::size_t fields = payload[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast)
      auto* const* values = reinterpret_cast<void* const*>(payload + 1);
      for (std::size_t i = 0; i < fields; ++i) {
        const void* valuePtr = values[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (valuePtr == nullptr) { continue; }
        if (ObjectHeader* headerPtr = find_object_for_pointer(valuePtr)) { mark(headerPtr); }
      }
      // Mark per-instance attribute dict if present (slot at values[fields])
      const void* attrDictPtr = values[fields]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      if (attrDictPtr != nullptr) {
        if (ObjectHeader* ad = find_object_for_pointer(attrDictPtr)) { mark(ad); }
      }
      break;
    }
    case TypeTag::Dict: {
      auto* base = reinterpret_cast<unsigned char*>(header);
      auto* payload = reinterpret_cast<std::size_t*>(base + sizeof(ObjectHeader)); // len, cap, keys[], vals[]
      const std::size_t len = payload[0]; (void)len; // length not needed for marking, we scan up to cap
      const std::size_t cap = payload[1];
      auto** keys = reinterpret_cast<void**>(payload + 2);
      auto** vals = keys + cap;
      for (std::size_t i = 0; i < cap; ++i) {
        if (keys[i] != nullptr) {
          if (ObjectHeader* k = find_object_for_pointer(keys[i])) { mark(k); }
        }
        if (vals[i] != nullptr) {
          if (ObjectHeader* v = find_object_for_pointer(vals[i])) { mark(v); }
        }
      }
      break;
    }
  }
}

static void mark_from_roots() {
  for (void* const* slot : g_roots) {
    void* const ptr = *slot;
    if (ptr == nullptr) { continue; }
    auto* hdr = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(ptr) - sizeof(ObjectHeader)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    mark(hdr);
  }
}

static bool in_object_payload(ObjectHeader* header, const void* ptr) {
  const auto* begin = reinterpret_cast<const unsigned char*>(header) + sizeof(ObjectHeader); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto* end = reinterpret_cast<const unsigned char*>(header) + header->size; // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto* ptrBytes = reinterpret_cast<const unsigned char*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  return ptrBytes >= begin && ptrBytes < end;
}

static ObjectHeader* find_object_for_pointer(const void* ptr) {
  for (ObjectHeader* header = g_head; header != nullptr; header = header->next) {
    if (in_object_payload(header, ptr)) { return header; }
  }
  return nullptr;
}

static void mark_from_remembered_locked() {
  // g_mu must be held by caller when calling this
  std::vector<void*> tmp;
  {
    const std::lock_guard<std::mutex> lockGuard(g_rem_mu);
    tmp.swap(g_remembered);
  }
  for (const void* valuePtr : tmp) {
    if (valuePtr == nullptr) { continue; }
    if (ObjectHeader* header = find_object_for_pointer(valuePtr)) { mark(header); }
  }
}

static std::optional<std::pair<void*, void*>> get_stack_bounds_pair() {
  // Current thread only
#ifdef __APPLE__
  pthread_t self = pthread_self();
  void* stackaddr = pthread_get_stackaddr_np(self);
  const size_t stacksize = pthread_get_stacksize_np(self);
  if (stackaddr != nullptr && stacksize > 0U) {
    // On macOS, stackaddr is the HIGH address, stack grows down
    const std::uintptr_t highAddr = reinterpret_cast<std::uintptr_t>(stackaddr);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const std::uintptr_t lowAddr = reinterpret_cast<std::uintptr_t>(static_cast<unsigned char*>(stackaddr) - stacksize);
    return std::make_pair(reinterpret_cast<void*>(lowAddr), reinterpret_cast<void*>(highAddr));
  }
  return std::nullopt;
#elifdef __linux__
  pthread_attr_t attr;
  if (pthread_getattr_np(pthread_self(), &attr) != 0) { return std::nullopt; }
  void* stackaddr = nullptr; size_t stacksize = 0;
  if (pthread_attr_getstack(&attr, &stackaddr, &stacksize) != 0) { pthread_attr_destroy(&attr); return std::nullopt; }
  pthread_attr_destroy(&attr);
  const std::uintptr_t lowAddr = reinterpret_cast<std::uintptr_t>(stackaddr);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const std::uintptr_t highAddr = reinterpret_cast<std::uintptr_t>(static_cast<unsigned char*>(stackaddr) + stacksize);
  return std::make_pair(reinterpret_cast<void*>(lowAddr), reinterpret_cast<void*>(highAddr));
#else
  // Fallback: approximate using address of local variable as low bound and assume 8MB stack
  unsigned char local{};
  const std::uintptr_t lowAddr = reinterpret_cast<std::uintptr_t>(&local);
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const std::uintptr_t highAddr = lowAddr + (8U << 20U);
  return std::make_pair(reinterpret_cast<void*>(lowAddr), reinterpret_cast<void*>(highAddr));
#endif
}

// NOLINTNEXTLINE(readability-function-size)
static void mark_from_stack() {
  const auto bounds = get_stack_bounds_pair();
  if (!bounds) { return; }
  // Align low up to word alignment
  std::uintptr_t lowAddr = reinterpret_cast<std::uintptr_t>(bounds->first); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::size_t align = alignof(std::uintptr_t);
  lowAddr = (lowAddr + (align - 1U)) & ~(static_cast<std::uintptr_t>(align) - 1U);
  auto* scanPtr = reinterpret_cast<std::uintptr_t*>(lowAddr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  auto* end = reinterpret_cast<std::uintptr_t*>(bounds->second);        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  // Iterate word-sized chunks across the stack region
  while (scanPtr < end) {
    const void* candidate = reinterpret_cast<void*>(*scanPtr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (candidate != nullptr) {
      if (ObjectHeader* header = find_object_for_pointer(candidate)) { mark(header); }
    }
    ++scanPtr; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
}

// NOLINTNEXTLINE(readability-function-size)
static bool mark_from_stack_slice(std::size_t words_budget) {
  if (g_stack_scan_cur == nullptr || g_stack_scan_end == nullptr) {
    const auto bounds = get_stack_bounds_pair();
    if (!bounds) { return true; }
    std::uintptr_t lowAddr = reinterpret_cast<std::uintptr_t>(bounds->first); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::size_t align = alignof(std::uintptr_t);
    lowAddr = (lowAddr + (align - 1U)) & ~(static_cast<std::uintptr_t>(align) - 1U);
    g_stack_scan_cur = reinterpret_cast<std::uintptr_t*>(lowAddr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    g_stack_scan_end = reinterpret_cast<std::uintptr_t*>(bounds->second);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  }
  std::size_t wordsScanned = 0;
  while (g_stack_scan_cur < g_stack_scan_end && wordsScanned < words_budget) {
    const void* candidate = reinterpret_cast<void*>(*g_stack_scan_cur); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (candidate != nullptr) {
      if (ObjectHeader* header = find_object_for_pointer(candidate)) { mark(header); }
    }
    ++g_stack_scan_cur; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ++wordsScanned;
  }
  return g_stack_scan_cur >= g_stack_scan_end;
}

// NOLINTNEXTLINE(readability-function-size)
static void sweep() {
  ObjectHeader* prev = nullptr; ObjectHeader* cur = g_head;
  std::size_t reclaimed = 0;
  while (cur != nullptr) {
    if (cur->mark == 0U) {
      ObjectHeader* dead = cur;
      cur = cur->next;
      if (prev != nullptr) { prev->next = cur; } else { g_head = cur; }
      reclaimed += dead->size;
      free_obj(dead);
    } else {
      // Survivor: clear mark and update generation/age
      cur->mark = 0; // clear for next cycle
      if (cur->gen == 0) {
        if (cur->age < 2) cur->age += 1;
        if (cur->age >= 1) { cur->gen = 1; }
      }
      prev = cur;
      cur = cur->next;
    }
  }
  g_stats.lastReclaimedBytes = static_cast<uint64_t>(reclaimed);
  g_stats.peakBytesLive = std::max(g_stats.peakBytesLive, g_stats.bytesLive);
}

void gc_collect() {
  // If background GC is enabled, request a cycle and wait until one completes
  if (g_bg_enabled.load(std::memory_order_relaxed)) {
    if (!g_bg_started) { start_bg_thread_if_needed(); }
    const uint64_t prev = g_gc_completed_count.load(std::memory_order_acquire);
    {
      const std::lock_guard<std::mutex> qlk(g_bg_mu);
      g_bg_requested.store(true, std::memory_order_relaxed);
      g_bg_cv.notify_one();
    }
    std::unique_lock<std::mutex> lk(g_gc_done_mu);
    g_gc_done_cv.wait(lk, [&]{ return g_gc_completed_count.load(std::memory_order_acquire) > prev; });
    return;
  }
  // Synchronous collection path
  const std::lock_guard<std::mutex> lock(g_mu);
  g_stats.numCollections++;
  mark_from_roots();
  if (g_conservative) { mark_from_stack(); }
  mark_from_remembered_locked();
  sweep();
}

void gc_set_threshold(std::size_t bytes) {
  const std::lock_guard<std::mutex> lock(g_mu);
  g_threshold = bytes;
}

void gc_set_conservative(bool enabled) {
  const std::lock_guard<std::mutex> lock(g_mu);
  g_conservative = enabled;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
static void start_bg_thread_if_needed() {
  if (g_bg_started) { return; }
  g_bg_started = true;
  g_bg_thread = std::thread([]() { // NOLINT(readability-function-cognitive-complexity)
    for (;;) {
      // Wait for request
      {
        std::unique_lock<std::mutex> lock(g_bg_mu);
        g_bg_cv.wait(lock, [] { return g_bg_requested.load(std::memory_order_relaxed); });
        g_bg_requested.store(false, std::memory_order_relaxed);
      }
      // Phases: Mark, then Sweep in slices
      auto slice_budget = std::chrono::microseconds(static_cast<long long>(g_slice_us.load(std::memory_order_relaxed)));
      // Marking
      {
        const std::lock_guard<std::mutex> lock2(g_mu);
        g_stats.numCollections++;
        mark_from_roots();
        mark_from_remembered_locked();
      }
      if (g_conservative) {
        auto tStart = std::chrono::steady_clock::now();
        while (true) {
          const bool done = mark_from_stack_slice(kStackSliceWords);
          if (done) { break; }
          if (std::chrono::steady_clock::now() - tStart > slice_budget) { std::this_thread::yield(); tStart = std::chrono::steady_clock::now(); }
        }
        // Reset scan pointers
        g_stack_scan_cur = nullptr; g_stack_scan_end = nullptr;
        // Finalize with any remembered writes while marking
        const std::lock_guard<std::mutex> lock2(g_mu);
        mark_from_remembered_locked();
      }
      // Sweeping in slices while holding g_mu briefly
      const std::chrono::nanoseconds min_hold(kMinLockHoldNs); // small lock hold
      for (;;) {
        auto t_lock_start = std::chrono::steady_clock::now();
        std::size_t reclaimed = 0;
        {
          const std::lock_guard<std::mutex> lock3(g_mu);
          if (g_sweep_cur == nullptr) { g_sweep_prev = nullptr; g_sweep_cur = g_head; }
          std::size_t steps = 0;
          const std::size_t batchSz = g_sweep_batch.load(std::memory_order_relaxed);
          while (g_sweep_cur != nullptr && steps < batchSz) { // sweep batch
            if (g_sweep_cur->mark == 0U) {
              ObjectHeader* dead = g_sweep_cur;
              g_sweep_cur = g_sweep_cur->next;
              if (g_sweep_prev != nullptr) { g_sweep_prev->next = g_sweep_cur; } else { g_head = g_sweep_cur; }
              reclaimed += dead->size;
              free_obj(dead);
            } else {
              g_sweep_cur->mark = 0;
              g_sweep_prev = g_sweep_cur;
              g_sweep_cur = g_sweep_cur->next;
            }
            ++steps;
          }
          g_stats.lastReclaimedBytes = static_cast<uint64_t>(reclaimed);
          g_stats.peakBytesLive = std::max(g_stats.peakBytesLive, g_stats.bytesLive);
        }
        if (g_sweep_cur == nullptr) { break; }
        if (std::chrono::steady_clock::now() - t_lock_start < min_hold) { std::this_thread::yield(); }
      }
      adapt_controller();
      // Done sweep
      // Notify any synchronous waiters that a GC completed
      {
        std::lock_guard<std::mutex> lk(g_gc_done_mu);
        g_gc_completed_count.fetch_add(1, std::memory_order_release);
      }
      g_gc_done_cv.notify_all();
    }
  });
  g_bg_thread.detach();
}

void gc_set_background(bool enabled) {
  const std::lock_guard<std::mutex> lock(g_mu);
  g_bg_enabled.store(enabled, std::memory_order_relaxed);
  if (enabled) { start_bg_thread_if_needed(); }
}

extern "C" void pycc_gc_write_barrier(void** slot, void* value) {
  gc_write_barrier(slot, value);
}

void gc_write_barrier(void** /*slot*/, void* value) {
  if (!g_bg_enabled.load(std::memory_order_relaxed)) { return; }
  if (value == nullptr) { return; }
  // Record the new value for later marking; avoid heavy locking
  const std::lock_guard<std::mutex> lockGuard(g_rem_mu);
  g_remembered.push_back(value);
}

void gc_pre_barrier(void** slot) {
  if (!g_bg_enabled.load(std::memory_order_relaxed)) { return; }
  if (g_barrier_mode.load(std::memory_order_relaxed) != 1) { return; } // SATB only
  if (slot == nullptr) { return; }
  // The slot may contain an indeterminate value before first initialization
  // in some callers (e.g., appending to a freshly grown list). That's fine for
  // SATB barriers: we treat unknown as not needing recording. Suppress the
  // analyzer's false positive on reading an indeterminate slot here.
  void* old = nullptr;
  std::memcpy(&old, slot, sizeof(void*)); // suppress analyzer false-positive for uninitialized read
  if (old == nullptr) { return; }
  const std::lock_guard<std::mutex> lockGuard(g_rem_mu);
  g_remembered.push_back(old);
}

void gc_set_barrier_mode(int mode) {
  g_barrier_mode.store((mode != 0) ? 1 : 0, std::memory_order_relaxed);
}

// C ABI wrappers for list/object to simplify IR calls later
extern "C" void* pycc_list_new(uint64_t cap) { return list_new(static_cast<std::size_t>(cap)); }
extern "C" void pycc_list_push(void** list_slot, void* elem) { list_push_slot(list_slot, elem); }
extern "C" uint64_t pycc_list_len(void* list) { return static_cast<uint64_t>(list_len(list)); }
extern "C" void* pycc_list_get(void* list, int64_t index) {
  if (list == nullptr) { return nullptr; }
  auto* meta = reinterpret_cast<std::size_t*>(list);
  std::size_t len = meta[0];
  int64_t idx = index;
  if (idx < 0) { idx += static_cast<int64_t>(len); }
  if (idx < 0) { return nullptr; }
  return list_get(list, static_cast<std::size_t>(idx));
}
extern "C" void pycc_list_set(void* list, int64_t index, void* value) {
  if (list == nullptr) { return; }
  auto* meta = reinterpret_cast<std::size_t*>(list);
  std::size_t len = meta[0];
  int64_t idx = index;
  if (idx < 0) { idx += static_cast<int64_t>(len); }
  if (idx < 0) { return; }
  list_set(list, static_cast<std::size_t>(idx), value);
}

// Dict interop
extern "C" void* pycc_dict_new(uint64_t cap) { return dict_new(static_cast<std::size_t>(cap)); }
extern "C" void pycc_dict_set(void** dict_slot, void* key, void* value) { dict_set(dict_slot, key, value); }
extern "C" void* pycc_dict_get(void* dict, void* key) { return dict_get(dict, key); }
extern "C" uint64_t pycc_dict_len(void* dict) { return static_cast<uint64_t>(dict_len(dict)); }

extern "C" void* pycc_object_new(uint64_t fields) { return object_new(static_cast<std::size_t>(fields)); }
extern "C" void pycc_object_set(void* obj, uint64_t idx, void* val) { object_set(obj, static_cast<std::size_t>(idx), val); }
extern "C" void* pycc_object_get(void* obj, uint64_t idx) { return object_get(obj, static_cast<std::size_t>(idx)); }
extern "C" void pycc_object_set_attr(void* obj, void* key_string, void* value) { object_set_attr(obj, key_string, value); }
extern "C" void* pycc_object_get_attr(void* obj, void* key_string) { return object_get_attr(obj, key_string); }

extern "C" void* pycc_box_int(int64_t value) { return box_int(value); }
extern "C" void* pycc_box_float(double value) { return box_float(value); }
extern "C" void* pycc_box_bool(bool value) { return box_bool(value); }
extern "C" void* pycc_string_new(const char* data, size_t length) { return string_new(data, length); }
extern "C" uint64_t pycc_string_len(void* str) { return static_cast<uint64_t>(string_len(str)); }
extern "C" uint64_t pycc_string_charlen(void* str);
extern "C" uint64_t pycc_string_charlen(void* str) { return static_cast<uint64_t>(string_charlen(str)); }
extern "C" void* pycc_string_slice(void* s, int64_t start, int64_t len) {
  std::size_t L = string_charlen(s);
  int64_t st = start; if (st < 0) st += static_cast<int64_t>(L); if (st < 0) st = 0; if (static_cast<std::size_t>(st) > L) st = static_cast<int64_t>(L);
  int64_t ln = len; if (ln < 0) ln = 0; if (static_cast<std::size_t>(st + ln) > L) ln = static_cast<int64_t>(L - static_cast<std::size_t>(st));
  return string_slice(s, static_cast<std::size_t>(st), static_cast<std::size_t>(ln));
}
void* string_repeat(void* s, std::size_t n) {
  if (n == 0) return string_new("", 0);
  const char* d = string_data(s);
  const std::size_t L = string_len(s);
  std::string tmp; tmp.resize(L * n);
  for (std::size_t i = 0; i < n; ++i) { if (L) std::memcpy(tmp.data() + (i * L), d, L); }
  return string_new(tmp.data(), tmp.size());
}
extern "C" void* pycc_string_repeat(void* s, int64_t n) {
  if (n <= 0) return string_new("", 0);
  return string_repeat(s, static_cast<std::size_t>(n));
}
extern "C" int pycc_string_contains(void* haystack, void* needle) {
  if (haystack == nullptr || needle == nullptr) return 0;
  const char* h = string_data(haystack);
  const char* n = string_data(needle);
  std::size_t lh = string_len(haystack), ln = string_len(needle);
  if (ln == 0) return 1;
  if (lh < ln) return 0;
  for (std::size_t i = 0; i + ln <= lh; ++i) { if (std::memcmp(h + i, n, ln) == 0) return 1; }
  return 0;
}

// C++ wrapper to match public header API
bool string_contains(void* haystack, void* needle) {
  return pycc_string_contains(haystack, needle) != 0;
}

// Exception wrappers for codegen (C ABI)
extern "C" void pycc_rt_raise(const char* type_name, const char* message) { rt_raise(type_name, message); }
extern "C" int pycc_rt_has_exception(void) { return rt_has_exception() ? 1 : 0; }
extern "C" void* pycc_rt_current_exception(void) { return rt_current_exception(); }
extern "C" void pycc_rt_clear_exception(void) { rt_clear_exception(); }
extern "C" void* pycc_rt_exception_type(void* exc) { return rt_exception_type(exc); }
extern "C" void* pycc_rt_exception_message(void* exc) { return rt_exception_message(exc); }

// String equality by content
static bool string_eq(void* a, void* b) {
  if (a == b) { return true; }
  if (a == nullptr || b == nullptr) { return false; }
  std::size_t la = string_len(a), lb = string_len(b);
  if (la != lb) { return false; }
  const char* da = string_data(a);
  const char* db = string_data(b);
  if (la == 0) { return true; }
  return std::memcmp(da, db, la) == 0;
}
extern "C" int pycc_string_eq(void* a, void* b) { return string_eq(a, b) ? 1 : 0; }

void gc_register_root(void** addr) {
  const std::lock_guard<std::mutex> lock(g_mu);
  g_roots.push_back(addr);
}

void gc_unregister_root(void** addr) {
  const std::lock_guard<std::mutex> lock(g_mu);
  auto iter = std::find(g_roots.begin(), g_roots.end(), addr);
  if (iter != g_roots.end()) { g_roots.erase(iter); }
}

RuntimeStats gc_stats() {
  const std::lock_guard<std::mutex> lock(g_mu);
  return g_stats;
}

void gc_reset_for_tests() {
  const std::lock_guard<std::mutex> lock(g_mu);
  // free all
  ObjectHeader* cur = g_head; g_head = nullptr;
  while (cur != nullptr) { ObjectHeader* nextHeader = cur->next; ::operator delete(cur); cur = nextHeader; }
  g_roots.clear();
  g_stats = {};
  g_conservative = false;
  g_threshold = kDefaultThresholdBytes;
  g_sweep_cur = nullptr;
  g_sweep_prev = nullptr;
  g_stack_scan_cur = nullptr;
  g_stack_scan_end = nullptr;
}

void* string_new(const char* data, std::size_t len) {
  const std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = sizeof(StringPayload) + len + 1; // include NUL
  auto* payloadBytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::String));
  if (g_debug) { std::fprintf(stderr, "[runtime] string_new(len=%zu) g_head=%p\n", len, static_cast<void*>(g_head)); }
  void* payloadVoid = static_cast<void*>(payloadBytes);
  auto* plen = static_cast<std::size_t*>(payloadVoid);
  *plen = len;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  char* buf = reinterpret_cast<char*>(plen + 1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (len != 0U && data != nullptr) { std::memcpy(buf, data, len); }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  buf[len] = '\0';
  // Do not synchronously collect here; request background GC instead.
  maybe_request_bg_gc_unlocked();
  return payloadVoid; // return pointer to payload start as the object handle
}

std::size_t string_len(void* str) {
  if (str == nullptr) { return 0; }
  auto* plen = static_cast<std::size_t*>(str);
  return *plen;
}

static std::size_t utf8_codepoint_count(const char* data, std::size_t n) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < n; ) {
    unsigned char c = static_cast<unsigned char>(data[i]);
    if ((c & 0x80U) == 0) { ++i; ++count; continue; }
    if ((c & 0xE0U) == 0xC0U && (i+1)<n) { i += 2; ++count; continue; }
    if ((c & 0xF0U) == 0xE0U && (i+2)<n) { i += 3; ++count; continue; }
    if ((c & 0xF8U) == 0xF0U && (i+3)<n) { i += 4; ++count; continue; }
    ++i; ++count; // invalid, advance 1 byte
  }
  return count;
}

std::size_t string_charlen(void* str) {
  if (str == nullptr) { return 0; }
  auto* plen = static_cast<std::size_t*>(str);
  const char* data = reinterpret_cast<const char*>(plen + 1);
  return utf8_codepoint_count(data, *plen);
}

const char* string_data(void* str) {
  if (str == nullptr) { return nullptr; }
  auto* plen = static_cast<std::size_t*>(str);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return reinterpret_cast<const char*>(plen + 1);
}

void* string_from_cstr(const char* cstr) {
  if (cstr == nullptr) { return string_new("", 0); }
  const std::size_t len = std::strlen(cstr);
  return string_new(cstr, len);
}

void* string_concat(void* a, void* b) {
  const char* da = string_data(a);
  const char* db = string_data(b);
  std::size_t la = string_len(a);
  std::size_t lb = string_len(b);
  if (la == 0) return string_new(db, lb);
  if (lb == 0) return string_new(da, la);
  std::string tmp; tmp.resize(la + lb);
  std::memcpy(tmp.data(), da, la);
  std::memcpy(tmp.data() + la, db, lb);
  return string_new(tmp.data(), tmp.size());
}

void* string_slice(void* s, std::size_t start, std::size_t len) {
  const char* d = string_data(s);
  const std::size_t nb = string_len(s);
  // Map code point index to byte offset
  auto cp_to_byte = [&](std::size_t cpIndex) -> std::size_t {
    std::size_t i = 0; std::size_t cp = 0;
    while (i < nb && cp < cpIndex) {
      unsigned char c = static_cast<unsigned char>(d[i]);
      if ((c & 0x80U) == 0) i += 1;
      else if ((c & 0xE0U) == 0xC0U) i += 2;
      else if ((c & 0xF0U) == 0xE0U) i += 3;
      else if ((c & 0xF8U) == 0xF0U) i += 4;
      else i += 1;
      ++cp;
    }
    return i;
  };
  std::size_t bstart = cp_to_byte(start);
  std::size_t bend = cp_to_byte(start + len);
  if (bend < bstart) bend = bstart;
  return string_new(d + bstart, bend - bstart);
}

// UTF-8 validation helper
static inline bool utf8_is_cont(uint8_t c) { return (c & 0xC0U) == 0x80U; }
bool utf8_is_valid(const char* data, std::size_t len) {
  if (data == nullptr) { return false; }
  const uint8_t* s = reinterpret_cast<const uint8_t*>(data);
  const uint8_t* end = s + len;
  while (s < end) {
    uint8_t c = *s++;
    if (c < 0x80U) { continue; }
    if ((c >> 5U) == 0x6U) { // 110xxxxx
      if (s >= end) return false; uint8_t c1 = *s++; if (!utf8_is_cont(c1)) return false;
      if ((c & 0x1EU) == 0x0U) return false; // overlong
    } else if ((c >> 4U) == 0xEU) { // 1110xxxx
      if (end - s < 2) return false; uint8_t c1 = *s++; uint8_t c2 = *s++;
      if (!utf8_is_cont(c1) || !utf8_is_cont(c2)) return false;
      uint32_t cp = ((c & 0x0FU) << 12U) | ((c1 & 0x3FU) << 6U) | (c2 & 0x3FU);
      if (cp < 0x800U) return false; // overlong
      if (cp >= 0xD800U && cp <= 0xDFFFU) return false; // surrogate range
    } else if ((c >> 3U) == 0x1EU) { // 11110xxx
      if (end - s < 3) return false; uint8_t c1 = *s++; uint8_t c2 = *s++; uint8_t c3 = *s++;
      if (!utf8_is_cont(c1) || !utf8_is_cont(c2) || !utf8_is_cont(c3)) return false;
      uint32_t cp = ((c & 0x07U) << 18U) | ((c1 & 0x3FU) << 12U) | ((c2 & 0x3FU) << 6U) | (c3 & 0x3FU);
      if (cp < 0x10000U || cp > 0x10FFFFU) return false; // overlong or out of range
    } else {
      return false; // invalid leading byte
    }
  }
  return true;
}

// Boxed primitives
void* box_int(int64_t value) {
  const std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = sizeof(int64_t);
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Int));
  std::memcpy(bytes, &value, sizeof(value));
  maybe_request_bg_gc_unlocked();
  return bytes;
}

int64_t box_int_value(void* obj) {
  if (obj == nullptr) { return 0; }
  int64_t out{}; std::memcpy(&out, obj, sizeof(out)); return out;
}

void* box_float(double value) {
  const std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = sizeof(double);
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Float));
  std::memcpy(bytes, &value, sizeof(value));
  maybe_request_bg_gc_unlocked();
  return bytes;
}

double box_float_value(void* obj) {
  if (obj == nullptr) { return 0.0; }
  double out{}; std::memcpy(&out, obj, sizeof(out)); return out;
}

void* box_bool(bool value) {
  const std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = sizeof(uint8_t);
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Bool));
  uint8_t byteVal = value ? 1U : 0U;
  std::memcpy(bytes, &byteVal, sizeof(byteVal));
  maybe_request_bg_gc_unlocked();
  return bytes;
}

bool box_bool_value(void* obj) {
  if (obj == nullptr) { return false; }
  uint8_t byteVal{}; std::memcpy(&byteVal, obj, sizeof(byteVal)); return byteVal != 0U;
}

// Lists
static void* list_new_locked(std::size_t capacity) {
  const std::size_t payloadSize = (sizeof(std::size_t) * 2) + (capacity * sizeof(void*)); // len, cap, items[]
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::List));
  auto* meta = reinterpret_cast<std::size_t*>(bytes); // NOLINT
  meta[0] = 0; meta[1] = capacity; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto** items = reinterpret_cast<void**>(meta + 2); // NOLINT
  for (std::size_t i = 0; i < capacity; ++i) { items[i] = nullptr; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  maybe_request_bg_gc_unlocked();
  return bytes;
}

void* list_new(std::size_t capacity) {
  const std::lock_guard<std::mutex> lock(g_mu);
  return list_new_locked(capacity);
}

void list_push_slot(void** list_slot, void* elem) {
  if (list_slot == nullptr) { return; }
  const std::lock_guard<std::mutex> lock(g_mu);
  auto* list = *list_slot;
  if (list == nullptr) {
    list = list_new_locked(kDefaultListCapacity);
    // update slot with barrier
    gc_pre_barrier(list_slot);
    gc_write_barrier(list_slot, list);
    *list_slot = list;
  }
  auto* meta = reinterpret_cast<std::size_t*>(list); // NOLINT
  const std::size_t len = meta[0];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const std::size_t cap = meta[1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto** items = reinterpret_cast<void**>(meta + 2); // NOLINT
  if (len >= cap) {
    const std::size_t newCap = (cap == 0U) ? kDefaultListCapacity : (cap * 2U);
    const std::size_t payloadSize = (sizeof(std::size_t) * 2) + (newCap * sizeof(void*));
    auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::List));
    auto* newMeta = reinterpret_cast<std::size_t*>(bytes); // NOLINT
    newMeta[0] = len; newMeta[1] = newCap; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto** newItems = reinterpret_cast<void**>(newMeta + 2); // NOLINT
    for (std::size_t i = 0; i < len; ++i) { newItems[i] = items[i]; }     // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (std::size_t i = len; i < newCap; ++i) { newItems[i] = nullptr; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    // Update external slot; barrier records reference
    gc_pre_barrier(list_slot);
    gc_write_barrier(list_slot, bytes);
    *list_slot = bytes;
    meta = newMeta; items = newItems; // 'list' and 'cap' are not used below
  }
  // write element; barrier on interior pointer slot
  gc_pre_barrier(&items[len]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  items[len] = elem;           // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  gc_write_barrier(&items[len], elem); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  meta[0] = len + 1; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  maybe_request_bg_gc_unlocked();
}

std::size_t list_len(void* list) {
  if (list == nullptr) { return 0; }
  const auto* meta = reinterpret_cast<const std::size_t*>(list); // NOLINT
  return meta[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

void* list_get(void* list, std::size_t index) {
  if (list == nullptr) { return nullptr; }
  const auto* meta = reinterpret_cast<const std::size_t*>(list); // NOLINT
  const std::size_t len = meta[0];
  if (index >= len) { return nullptr; }
  auto* const* items = reinterpret_cast<void* const*>(meta + 2); // NOLINT
  return items[index]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

void list_set(void* list, std::size_t index, void* value) {
  if (list == nullptr) { return; }
  const std::lock_guard<std::mutex> lock(g_mu);
  auto* meta = reinterpret_cast<std::size_t*>(list); // NOLINT
  const std::size_t len = meta[0];
  if (index >= len) { return; }
  auto** items = reinterpret_cast<void**>(meta + 2); // NOLINT
  gc_pre_barrier(&items[index]);
  items[index] = value;
  gc_write_barrier(&items[index], value);
}

// Dicts: open-addressed hash table (linear probe, pointer-identity keys)
static std::size_t ptr_hash(void* p) {
  auto v = reinterpret_cast<std::uintptr_t>(p);
  // 64-bit mix
  v ^= v >> 33; v *= 0xff51afd7ed558ccdULL; v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ULL; v ^= v >> 33;
  return static_cast<std::size_t>(v);
}

static void* dict_new_locked(std::size_t capacity) {
  if (capacity < 8) { capacity = 8; }
  const std::size_t payloadSize = (sizeof(std::size_t) * 2) + (capacity * sizeof(void*)) + (capacity * sizeof(void*));
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Dict));
  auto* meta = reinterpret_cast<std::size_t*>(bytes);
  meta[0] = 0; meta[1] = capacity; // len, cap
  auto** keys = reinterpret_cast<void**>(meta + 2);
  auto** vals = keys + capacity;
  for (std::size_t i = 0; i < capacity; ++i) { keys[i] = nullptr; vals[i] = nullptr; }
  maybe_request_bg_gc_unlocked();
  return bytes;
}

void* dict_new(std::size_t capacity) {
  const std::lock_guard<std::mutex> lock(g_mu);
  return dict_new_locked(capacity);
}

static void dict_rehash(void** dict_slot, std::size_t newCap) {
  auto* old = *dict_slot;
  auto* meta = reinterpret_cast<std::size_t*>(old);
  const std::size_t cap = meta[1];
  auto** keys = reinterpret_cast<void**>(meta + 2);
  auto** vals = keys + cap;
  auto* bytes = static_cast<unsigned char*>(alloc_raw((sizeof(std::size_t) * 2) + (newCap * sizeof(void*)) + (newCap * sizeof(void*)), TypeTag::Dict));
  auto* nmeta = reinterpret_cast<std::size_t*>(bytes);
  nmeta[0] = 0; nmeta[1] = newCap;
  auto** nkeys = reinterpret_cast<void**>(nmeta + 2);
  auto** nvals = nkeys + newCap;
  for (std::size_t i = 0; i < newCap; ++i) { nkeys[i] = nullptr; nvals[i] = nullptr; }
  // reinsertion
  for (std::size_t i = 0; i < cap; ++i) {
    if (keys[i] == nullptr) continue;
    std::size_t idx = ptr_hash(keys[i]) & (newCap - 1);
    while (nkeys[idx] != nullptr) { idx = (idx + 1) & (newCap - 1); }
    nkeys[idx] = keys[i]; nvals[idx] = vals[i]; nmeta[0]++;
  }
  gc_pre_barrier(dict_slot);
  gc_write_barrier(dict_slot, bytes);
  *dict_slot = bytes;
}

void dict_set(void** dict_slot, void* key, void* value) {
  if (dict_slot == nullptr) { return; }
  const std::lock_guard<std::mutex> lock(g_mu);
  if (*dict_slot == nullptr) { *dict_slot = dict_new_locked(8); }
  auto* meta = reinterpret_cast<std::size_t*>(*dict_slot);
  std::size_t len = meta[0]; const std::size_t cap = meta[1];
  auto** keys = reinterpret_cast<void**>(meta + 2);
  auto** vals = keys + cap;
  // grow if load factor > 0.7 (cap should be power of two for masking)
  if ((len + 1) * 10 > cap * 7) { dict_rehash(dict_slot, cap * 2); meta = reinterpret_cast<std::size_t*>(*dict_slot); len = meta[0]; }
  const std::size_t ncap = meta[1]; keys = reinterpret_cast<void**>(meta + 2); vals = keys + ncap;
  std::size_t idx = ptr_hash(key) & (ncap - 1);
  while (true) {
    if (keys[idx] == nullptr || keys[idx] == key) {
      if (keys[idx] == nullptr) { meta[0] = len + 1; }
      gc_pre_barrier(&keys[idx]); keys[idx] = key; gc_write_barrier(&keys[idx], key);
      gc_pre_barrier(&vals[idx]); vals[idx] = value; gc_write_barrier(&vals[idx], value);
      break;
    }
    idx = (idx + 1) & (ncap - 1);
  }
  maybe_request_bg_gc_unlocked();
}

void* dict_get(void* dict, void* key) {
  if (dict == nullptr) { return nullptr; }
  auto* meta = reinterpret_cast<std::size_t*>(dict);
  const std::size_t cap = meta[1];
  auto** keys = reinterpret_cast<void**>(meta + 2);
  auto** vals = keys + cap;
  std::size_t idx = ptr_hash(key) & (cap - 1);
  for (;;) {
    if (keys[idx] == nullptr) { return nullptr; }
    if (keys[idx] == key) { return vals[idx]; }
    idx = (idx + 1) & (cap - 1);
  }
}

std::size_t dict_len(void* dict) {
  if (dict == nullptr) { return 0; }
  auto* meta = reinterpret_cast<std::size_t*>(dict);
  return meta[0];
}

extern "C" void* pycc_dict_iter_new(void* dict) {
  const std::lock_guard<std::mutex> lock(g_mu);
  void* it = object_new(2);
  auto* meta = reinterpret_cast<std::size_t*>(it);
  auto** vals = reinterpret_cast<void**>(meta + 1);
  gc_pre_barrier(&vals[0]); vals[0] = dict; gc_write_barrier(&vals[0], dict);
  void* zero = box_int(0);
  gc_pre_barrier(&vals[1]); vals[1] = zero; gc_write_barrier(&vals[1], zero);
  return it;
}

extern "C" void* pycc_dict_iter_next(void* it) {
  if (it == nullptr) { return nullptr; }
  const std::lock_guard<std::mutex> lock(g_mu);
  auto* meta_it = reinterpret_cast<std::size_t*>(it);
  auto** vals_it = reinterpret_cast<void**>(meta_it + 1);
  void* dict = vals_it[0]; if (dict == nullptr) return nullptr;
  auto* meta = reinterpret_cast<std::size_t*>(dict);
  const std::size_t cap = meta[1];
  auto** keys = reinterpret_cast<void**>(meta + 2);
  std::size_t idx = static_cast<std::size_t>(box_int_value(vals_it[1]));
  while (idx < cap) {
    void* k = keys[idx];
    ++idx;
    if (k != nullptr) { vals_it[1] = box_int(static_cast<int64_t>(idx)); return k; }
  }
  vals_it[1] = box_int(static_cast<int64_t>(cap));
  return nullptr;
}

// C++ API wrappers to match header declarations
namespace pycc::rt {
void* dict_iter_new(void* dict) { return pycc_dict_iter_new(dict); }
void* dict_iter_next(void* it) { return pycc_dict_iter_next(it); }
}

// Objects (fixed-size field table)
void* object_new(std::size_t field_count) {
  const std::lock_guard<std::mutex> lock(g_mu);
  // Allocate extra slot for per-instance attribute dict pointer at values[fields]
  const std::size_t payloadSize = sizeof(std::size_t) + ((field_count + 1) * sizeof(void*)); // fields, values[] (+attrs)
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Object));
  auto* meta = reinterpret_cast<std::size_t*>(bytes); // NOLINT
  meta[0] = field_count; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto** vals = reinterpret_cast<void**>(meta + 1); // NOLINT
  for (std::size_t i = 0; i < field_count + 1; ++i) { vals[i] = nullptr; } // values + attrs slot
  maybe_request_bg_gc_unlocked();
  return bytes;
}

void object_set(void* obj, std::size_t index, void* value) {
  if (obj == nullptr) { return; }
  const std::lock_guard<std::mutex> lock(g_mu);
  auto* meta = reinterpret_cast<std::size_t*>(obj); // NOLINT
  const std::size_t fields = meta[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (index >= fields) { return; }
  auto** vals = reinterpret_cast<void**>(meta + 1); // NOLINT
  gc_pre_barrier(&vals[index]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  vals[index] = value;          // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  gc_write_barrier(&vals[index], value); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

void* object_get(void* obj, std::size_t index) {
  if (obj == nullptr) { return nullptr; }
  auto* meta = reinterpret_cast<std::size_t*>(obj); // NOLINT
  const std::size_t fields = meta[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (index >= fields) { return nullptr; }
  auto* const* vals = reinterpret_cast<void* const*>(meta + 1); // NOLINT
  return vals[index]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

std::size_t object_field_count(void* obj) {
  if (obj == nullptr) { return 0; }
  const auto* meta = reinterpret_cast<const std::size_t*>(obj); // NOLINT
  return meta[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

static inline void** object_attrs_slot(void* obj) {
  auto* meta = reinterpret_cast<std::size_t*>(obj);
  auto** vals = reinterpret_cast<void**>(meta + 1);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return &vals[meta[0]]; // slot after values[]
}

void* object_get_attr_dict(void* obj) {
  if (obj == nullptr) { return nullptr; }
  auto** slot = object_attrs_slot(obj);
  return *slot;
}

void object_set_attr(void* obj, void* key_string, void* value) {
  if (obj == nullptr || key_string == nullptr) { return; }
  const std::lock_guard<std::mutex> lock(g_mu);
  auto** slot = object_attrs_slot(obj);
  if (*slot == nullptr) {
    // lazily create dict
    void* d = dict_new_locked(8);
    gc_pre_barrier(slot);
    gc_write_barrier(slot, d);
    *slot = d;
  }
  // set into dict
  dict_set(slot, key_string, value);
}

void* object_get_attr(void* obj, void* key_string) {
  if (obj == nullptr || key_string == nullptr) { return nullptr; }
  const std::lock_guard<std::mutex> lock(g_mu);
  auto** slot = object_attrs_slot(obj);
  if (*slot == nullptr) { return nullptr; }
  return dict_get(*slot, key_string);
}
GcTelemetry gc_telemetry() {
  uint64_t live_now = 0; std::size_t thr = 0;
  {
    const std::lock_guard<std::mutex> lock(g_mu);
    live_now = g_stats.bytesLive;
    thr = g_threshold;
  }
  const double pressure = (thr != 0U) ? (static_cast<double>(live_now) / static_cast<double>(thr)) : 0.0;
  const double bps = g_ewma_alloc_rate * 1000.0; // bytes/ms -> bytes/s
  return GcTelemetry{bps, pressure};
}
static void adapt_controller() {
  // Heuristics with EWMA smoothing
  const uint64_t now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
  const uint64_t last_ms = g_last_time_ms.load(std::memory_order_relaxed);
  const uint64_t delta_ms = (now_ms > last_ms) ? (now_ms - last_ms) : 1U;
  g_last_time_ms.store(now_ms, std::memory_order_relaxed);

  const uint64_t last_alloc = g_last_bytes_alloc.load(std::memory_order_relaxed);
  uint64_t alloc_now = 0;
  {
    const std::lock_guard<std::mutex> lock(g_mu);
    alloc_now = g_stats.bytesAllocated;
  }
  const uint64_t delta_alloc = (alloc_now > last_alloc) ? (alloc_now - last_alloc) : 0U;
  g_last_bytes_alloc.store(alloc_now, std::memory_order_relaxed);

  const double alloc_rate = static_cast<double>(delta_alloc) / static_cast<double>(delta_ms + 1U); // bytes/ms
  double pressure = 0.0;
  {
    const std::lock_guard<std::mutex> lock(g_mu);
    pressure = (g_threshold != 0U) ? (static_cast<double>(g_stats.bytesLive) / static_cast<double>(g_threshold)) : 0.0;
  }
  // EWMA smoothing to avoid oscillations
  constexpr double alpha = 0.2;
  g_ewma_alloc_rate = (alpha * alloc_rate) + ((1.0 - alpha) * g_ewma_alloc_rate);
  g_ewma_pressure = (alpha * pressure) + ((1.0 - alpha) * g_ewma_pressure);

  // Adjust slice and sweep batch
  uint64_t slice = g_slice_us.load(std::memory_order_relaxed);
  std::size_t batch = g_sweep_batch.load(std::memory_order_relaxed);
  if (g_ewma_pressure > kHighPressure || g_ewma_alloc_rate > kHighAllocRateBytesPerMs) { // > 4KB/ms
    slice = std::min<uint64_t>(slice + kSliceIncrementUs, kMaxSliceUs); // cap ~5ms
    batch = std::min<std::size_t>(batch + kBatchIncrement, kMaxBatch);
  } else if (g_ewma_pressure < kLowPressure && g_ewma_alloc_rate < kLowAllocRateBytesPerMs) { // < 0.5KB/ms
    slice = (slice > kSliceLowerTriggerUs) ? (slice - kSliceDecrementUs) : kSliceDefaultUs;
    batch = (batch > kBatchLowerTrigger) ? (batch - kBatchDecrement) : kBatchDefault;
  }
  g_slice_us.store(slice, std::memory_order_relaxed);
  g_sweep_batch.store(batch, std::memory_order_relaxed);
}

// Exceptions implementation: minimal per-thread exception object with type and message
void rt_raise(const char* type_name, const char* message) {
  // allocate objects under lock to integrate with GC lists
  const std::lock_guard<std::mutex> lock(g_mu);
  void* t = string_from_cstr(type_name);
  void* m = string_from_cstr(message);
  void* exc = object_new(2);
  auto* meta = reinterpret_cast<std::size_t*>(exc);
  auto** vals = reinterpret_cast<void**>(meta + 1);
  gc_pre_barrier(&vals[0]); vals[0] = t; gc_write_barrier(&vals[0], t);
  gc_pre_barrier(&vals[1]); vals[1] = m; gc_write_barrier(&vals[1], m);
  t_last_exception = exc;
  if (!t_exc_root_registered) { gc_register_root(&t_last_exception); t_exc_root_registered = true; }
  // Phase 1 EH: throw a lightweight C++ exception instance
  struct PyccCppExcLocal : public std::exception { const char* what() const noexcept override { return "pycc"; } };
  throw PyccCppExcLocal();
}

bool rt_has_exception() { return t_last_exception != nullptr; }
void* rt_current_exception() { return t_last_exception; }
void rt_clear_exception() {
  if (t_exc_root_registered) { gc_unregister_root(&t_last_exception); t_exc_root_registered = false; }
  t_last_exception = nullptr;
}

void* rt_exception_type(void* exc) {
  if (exc == nullptr) { return nullptr; }
  auto* meta = reinterpret_cast<std::size_t*>(exc);
  auto** vals = reinterpret_cast<void**>(meta + 1);
  return vals[0];
}

void* rt_exception_message(void* exc) {
  if (exc == nullptr) { return nullptr; }
  auto* meta = reinterpret_cast<std::size_t*>(exc);
  auto** vals = reinterpret_cast<void**>(meta + 1);
  return vals[1];
}

// I/O and OS interop
void io_write_stdout(void* str) {
  const char* data = string_data(str);
  const std::size_t len = string_len(str);
  if (data != nullptr && len > 0) { std::fwrite(data, 1, len, stdout); }
}

void io_write_stderr(void* str) {
  const char* data = string_data(str);
  const std::size_t len = string_len(str);
  if (data != nullptr && len > 0) { std::fwrite(data, 1, len, stderr); }
}

void* io_read_file(const char* path) {
  if (path == nullptr) { return nullptr; }
  FILE* f = std::fopen(path, "rb");
  if (!f) { return nullptr; }
  if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return nullptr; }
  long sz = std::ftell(f);
  if (sz < 0) { std::fclose(f); return nullptr; }
  if (std::fseek(f, 0, SEEK_SET) != 0) { std::fclose(f); return nullptr; }
  std::vector<char> buf(static_cast<std::size_t>(sz));
  std::size_t n = (sz == 0) ? 0 : std::fread(buf.data(), 1, static_cast<std::size_t>(sz), f);
  std::fclose(f);
  if (n != static_cast<std::size_t>(sz)) { return nullptr; }
  return string_new(buf.data(), buf.size());
}

bool io_write_file(const char* path, void* str) {
  if (path == nullptr) { return false; }
  const char* data = string_data(str);
  const std::size_t len = string_len(str);
  FILE* f = std::fopen(path, "wb");
  if (!f) { return false; }
  std::size_t n = (len == 0) ? 0 : std::fwrite(data, 1, len, f);
  std::fclose(f);
  return n == len;
}

// C ABI wrappers for io module
extern "C" void pycc_io_write_stdout(void* str) { ::pycc::rt::io_write_stdout(str); }
extern "C" void pycc_io_write_stderr(void* str) { ::pycc::rt::io_write_stderr(str); }
extern "C" void* pycc_io_read_file(void* pathStr) {
  const char* p = ::pycc::rt::string_data(pathStr);
  if (!p) return nullptr;
  return ::pycc::rt::io_read_file(p);
}
extern "C" bool pycc_io_write_file(void* pathStr, void* str) {
  const char* p = ::pycc::rt::string_data(pathStr);
  if (!p) return false;
  return ::pycc::rt::io_write_file(p, str);
}

void* os_getenv(const char* name) {
  if (name == nullptr) { return nullptr; }
  const char* val = std::getenv(name);
  if (val == nullptr) { return nullptr; }
  return string_from_cstr(val);
}

int64_t os_time_ms() {
  using namespace std::chrono;
  auto now = time_point_cast<milliseconds>(system_clock::now());
  return now.time_since_epoch().count();
}

// Time module shims
double time_time() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto secs = duration_cast<duration<double>>(now.time_since_epoch());
  return secs.count();
}
int64_t time_time_ns() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto ns = duration_cast<nanoseconds>(now.time_since_epoch());
  return static_cast<int64_t>(ns.count());
}
double time_monotonic() {
  using namespace std::chrono;
  auto now = steady_clock::now();
  static const auto start = now;
  return duration_cast<duration<double>>(now - start).count();
}
int64_t time_monotonic_ns() {
  using namespace std::chrono;
  auto now = steady_clock::now();
  static const auto start = now;
  return static_cast<int64_t>(duration_cast<nanoseconds>(now - start).count());
}
double time_perf_counter() {
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  static const auto start = now;
  return duration_cast<duration<double>>(now - start).count();
}
int64_t time_perf_counter_ns() {
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  static const auto start = now;
  return static_cast<int64_t>(duration_cast<nanoseconds>(now - start).count());
}
double time_process_time() {
  return static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC);
}
void time_sleep(double seconds) {
  if (seconds <= 0.0) return;
  using namespace std::chrono;
  auto dur = duration<double>(seconds);
  auto ms = duration_cast<milliseconds>(dur);
  std::this_thread::sleep_for(ms);
}

static void* make_iso8601_from_tm(const std::tm& tm, int sec) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, sec);
  return string_from_cstr(buf);
}
void* datetime_now() {
  std::time_t t = std::time(nullptr);
  std::tm localTm{};
#if defined(_WIN32)
  localtime_s(&localTm, &t);
#else
  localtime_r(&t, &localTm);
#endif
  return make_iso8601_from_tm(localTm, localTm.tm_sec);
}
void* datetime_utcnow() {
  std::time_t t = std::time(nullptr);
  std::tm utcTm{};
#if defined(_WIN32)
  gmtime_s(&utcTm, &t);
#else
  gmtime_r(&t, &utcTm);
#endif
  return make_iso8601_from_tm(utcTm, utcTm.tm_sec);
}
void* datetime_fromtimestamp(double ts) {
  std::time_t t = static_cast<std::time_t>(ts);
  std::tm localTm{};
#if defined(_WIN32)
  localtime_s(&localTm, &t);
#else
  localtime_r(&t, &localTm);
#endif
  return make_iso8601_from_tm(localTm, localTm.tm_sec);
}
void* datetime_utcfromtimestamp(double ts) {
  std::time_t t = static_cast<std::time_t>(ts);
  std::tm utcTm{};
#if defined(_WIN32)
  gmtime_s(&utcTm, &t);
#else
  gmtime_r(&t, &utcTm);
#endif
  return make_iso8601_from_tm(utcTm, utcTm.tm_sec);
}

// ---- sys module shims ----
static thread_local int32_t t_last_exit_code = 0; // NOLINT
void* sys_platform() {
#if defined(__APPLE__)
  return string_from_cstr("darwin");
#elif defined(__linux__)
  return string_from_cstr("linux");
#elif defined(_WIN32)
  return string_from_cstr("win32");
#else
  return string_from_cstr("unknown");
#endif
}
void* sys_version() {
#if defined(__clang__)
  return string_from_cstr("pycc/clang");
#elif defined(__GNUC__)
  return string_from_cstr("pycc/gcc");
#else
  return string_from_cstr("pycc");
#endif
}
int64_t sys_maxsize() {
  // Approximate: pointer width decides; return 2^31-1 or 2^63-1
  if (sizeof(void*) >= 8) return static_cast<int64_t>(0x7FFFFFFFFFFFFFFFLL);
  return static_cast<int64_t>(0x7FFFFFFFLL);
}
void sys_exit(int32_t code) {
  t_last_exit_code = code;
  // In test environment, we don't actually exit the process.
}

// Bytes (immutable)
void* bytes_new(const void* data, std::size_t len) {
  const std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = sizeof(BytesPayload) + len;
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Bytes));
  auto* plen = reinterpret_cast<std::size_t*>(bytes);
  *plen = len;
  auto* buf = reinterpret_cast<unsigned char*>(plen + 1);
  if (len != 0U && data != nullptr) { std::memcpy(buf, data, len); }
  maybe_request_bg_gc_unlocked();
  return bytes;
}
std::size_t bytes_len(void* obj) {
  if (obj == nullptr) return 0; return *reinterpret_cast<std::size_t*>(obj);
}
const unsigned char* bytes_data(void* obj) {
  if (obj == nullptr) return nullptr; auto* plen = reinterpret_cast<std::size_t*>(obj); return reinterpret_cast<const unsigned char*>(plen + 1);
}
void* bytes_slice(void* obj, std::size_t start, std::size_t len) {
  const unsigned char* d = bytes_data(obj); const std::size_t L = bytes_len(obj);
  if (start > L) start = L; std::size_t n = (start + len > L) ? (L - start) : len; return bytes_new(d + start, n);
}
void* bytes_concat(void* a, void* b) {
  const std::size_t la = bytes_len(a), lb = bytes_len(b);
  const unsigned char* da = bytes_data(a); const unsigned char* db = bytes_data(b);
  std::vector<unsigned char> tmp; tmp.resize(la + lb);
  if (la) std::memcpy(tmp.data(), da, la);
  if (lb) std::memcpy(tmp.data() + la, db, lb);
  return bytes_new(tmp.data(), tmp.size());
}

// Encoding/decoding helpers (basic utf-8/ascii)
static const char* nonnull(const char* p, const char* defv) { return (p && *p) ? p : defv; }
void* string_encode(void* s, const char* encoding, const char* errors) {
  if (!s) return nullptr;
  const char* enc = nonnull(encoding, "utf-8");
  const char* err = nonnull(errors, "strict");
  (void)err;
  if (std::strcmp(enc, "utf-8") == 0) {
    return bytes_new(string_data(s), string_len(s));
  }
  if (std::strcmp(enc, "ascii") == 0) {
    const char* d = string_data(s);
    const std::size_t nb = string_len(s);
    std::vector<unsigned char> out; out.reserve(nb);
    for (std::size_t i=0; i<nb; ) {
      unsigned char c = static_cast<unsigned char>(d[i]);
      if ((c & 0x80U) == 0) { out.push_back(c); ++i; }
      else {
        if (std::strcmp(err, "replace") == 0) {
          out.push_back('?');
          if ((c & 0xE0U) == 0xC0U) i += 2; else if ((c & 0xF0U) == 0xE0U) i += 3; else if ((c & 0xF8U) == 0xF0U) i += 4; else i += 1;
        } else {
          rt_raise("UnicodeEncodeError", "ascii codec can't encode character");
          return nullptr;
        }
      }
    }
    return bytes_new(out.data(), out.size());
  }
  rt_raise("LookupError", "unknown encoding");
  return nullptr;
}
void* bytes_decode(void* b, const char* encoding, const char* errors) {
  if (!b) return nullptr;
  const char* enc = nonnull(encoding, "utf-8");
  const char* err = nonnull(errors, "strict");
  if (std::strcmp(enc, "utf-8") == 0) {
    const std::size_t nb = bytes_len(b);
    const unsigned char* p = bytes_data(b);
    if (!utf8_is_valid(reinterpret_cast<const char*>(p), nb)) {
      if (std::strcmp(err, "replace") == 0) {
        std::vector<unsigned char> repaired; repaired.reserve(nb);
        for (std::size_t i=0; i<nb; ) {
          unsigned char c = p[i];
          if ((c & 0x80U) == 0) { repaired.push_back(c); ++i; }
          else if ((c & 0xE0U) == 0xC0U && (i+1)<nb) { repaired.push_back(c); repaired.push_back(p[i+1]); i+=2; }
          else if ((c & 0xF0U) == 0xE0U && (i+2)<nb) { repaired.push_back(c); repaired.push_back(p[i+1]); repaired.push_back(p[i+2]); i+=3; }
          else if ((c & 0xF8U) == 0xF0U && (i+3)<nb) { repaired.push_back(c); repaired.push_back(p[i+1]); repaired.push_back(p[i+2]); repaired.push_back(p[i+3]); i+=4; }
          else { repaired.push_back(0xEFU); repaired.push_back(0xBFU); repaired.push_back(0xBDU); ++i; }
        }
        return string_new(reinterpret_cast<const char*>(repaired.data()), repaired.size());
      }
      rt_raise("UnicodeDecodeError", "invalid utf-8");
      return nullptr;
    }
    return string_new(reinterpret_cast<const char*>(p), nb);
  }
  if (std::strcmp(enc, "ascii") == 0) {
    const std::size_t nb = bytes_len(b);
    const unsigned char* p = bytes_data(b);
    for (std::size_t i=0; i<nb; ++i) {
      if ((p[i] & 0x80U) != 0) {
        if (std::strcmp(err, "replace") == 0) {
          std::vector<unsigned char> out; out.reserve(nb);
          for (std::size_t j=0; j<nb; ++j) out.push_back((p[j]&0x80U)?'?':p[j]);
          return string_new(reinterpret_cast<const char*>(out.data()), out.size());
        }
        rt_raise("UnicodeDecodeError", "ascii codec can't decode byte");
        return nullptr;
      }
    }
    return string_new(reinterpret_cast<const char*>(p), nb);
  }
  rt_raise("LookupError", "unknown encoding");
  return nullptr;
}

// Normalization and casefolding stubs (full ICU support behind PYCC_WITH_ICU)
void* string_normalize(void* s, NormalizationForm form) {
  if (!s) return nullptr;
#ifdef PYCC_WITH_ICU
  const char* data = string_data(s);
  const int32_t nb = static_cast<int32_t>(string_len(s));
  UErrorCode status = U_ZERO_ERROR;
  const char* mode = (form == NormalizationForm::NFC) ? "nfc"
                     : (form == NormalizationForm::NFD) ? "nfd"
                     : (form == NormalizationForm::NFKC) ? "nfkc" : "nfkd";
  const UNormalizer2* norm = unorm2_getInstance(nullptr, mode, UNORM2_COMPOSE, &status);
  if (U_FAILURE(status)) { return string_new(data, nb); }
  // Convert UTF-8 -> UTF-16
  int32_t uLen = 0; u_strFromUTF8(nullptr, 0, &uLen, data, nb, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { return string_new(data, nb); }
  status = U_ZERO_ERROR; std::vector<UChar> ustr(uLen + 1);
  u_strFromUTF8(ustr.data(), uLen + 1, nullptr, data, nb, &status);
  if (U_FAILURE(status)) { return string_new(data, nb); }
  // Normalize
  int32_t nLen = unorm2_normalize(norm, ustr.data(), uLen, nullptr, 0, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { return string_new(data, nb); }
  status = U_ZERO_ERROR; std::vector<UChar> normBuf(nLen + 1);
  unorm2_normalize(norm, ustr.data(), uLen, normBuf.data(), nLen + 1, &status);
  if (U_FAILURE(status)) { return string_new(data, nb); }
  // Convert UTF-16 -> UTF-8
  int32_t outLen = 0; u_strToUTF8(nullptr, 0, &outLen, normBuf.data(), nLen, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { return string_new(data, nb); }
  status = U_ZERO_ERROR; std::vector<char> out(outLen + 1);
  u_strToUTF8(out.data(), outLen + 1, nullptr, normBuf.data(), nLen, &status);
  if (U_FAILURE(status)) { return string_new(data, nb); }
  return string_new(out.data(), static_cast<std::size_t>(outLen));
#else
  (void)form;
  return string_new(string_data(s), string_len(s));
#endif
}
void* string_casefold(void* s) {
  if (!s) return nullptr;
#ifdef PYCC_WITH_ICU
  const char* data = string_data(s);
  const int32_t nb = static_cast<int32_t>(string_len(s));
  UErrorCode status = U_ZERO_ERROR;
  int32_t uLen = 0; u_strFromUTF8(nullptr, 0, &uLen, data, nb, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { return string_new(data, nb); }
  status = U_ZERO_ERROR; std::vector<UChar> ustr(uLen + 1);
  u_strFromUTF8(ustr.data(), uLen + 1, nullptr, data, nb, &status);
  if (U_FAILURE(status)) { return string_new(data, nb); }
  // Case fold (full)
  int32_t fLen = u_strFoldCase(nullptr, 0, ustr.data(), uLen, U_FOLD_CASE_DEFAULT, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { return string_new(data, nb); }
  status = U_ZERO_ERROR; std::vector<UChar> fbuf(fLen + 1);
  u_strFoldCase(fbuf.data(), fLen + 1, ustr.data(), uLen, U_FOLD_CASE_DEFAULT, &status);
  if (U_FAILURE(status)) { return string_new(data, nb); }
  // Convert back to UTF-8
  int32_t outLen = 0; u_strToUTF8(nullptr, 0, &outLen, fbuf.data(), fLen, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) { return string_new(data, nb); }
  status = U_ZERO_ERROR; std::vector<char> out(outLen + 1);
  u_strToUTF8(out.data(), outLen + 1, nullptr, fbuf.data(), fLen, &status);
  if (U_FAILURE(status)) { return string_new(data, nb); }
  return string_new(out.data(), static_cast<std::size_t>(outLen));
#else
  return string_new(string_data(s), string_len(s));
#endif
}

extern "C" void* pycc_string_encode(void* s, const char* enc, const char* err) { return string_encode(s, enc, err); }
extern "C" void* pycc_bytes_decode(void* b, const char* enc, const char* err) { return bytes_decode(b, enc, err); }

// ByteArray (mutable)
void* bytearray_new(std::size_t len) {
  const std::lock_guard<std::mutex> lock(g_mu);
  std::size_t cap = std::max<std::size_t>(len, 8);
  const std::size_t payloadSize = sizeof(ByteArrayPayload) + cap;
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::ByteArray));
  auto* hdr = reinterpret_cast<std::size_t*>(bytes);
  hdr[0] = len; hdr[1] = cap;
  auto* buf = reinterpret_cast<unsigned char*>(hdr + 2);
  std::memset(buf, 0, cap);
  maybe_request_bg_gc_unlocked();
  return bytes;
}
void* bytearray_from_bytes(void* b) {
  const std::size_t len = bytes_len(b);
  void* arr = bytearray_new(len);
  auto* hdr = reinterpret_cast<std::size_t*>(arr);
  auto* buf = reinterpret_cast<unsigned char*>(hdr + 2);
  std::memcpy(buf, bytes_data(b), len);
  return arr;
}
std::size_t bytearray_len(void* obj) { if (!obj) return 0; return reinterpret_cast<std::size_t*>(obj)[0]; }
static inline unsigned char* bytearray_buf(void* obj) { auto* hdr = reinterpret_cast<std::size_t*>(obj); return reinterpret_cast<unsigned char*>(hdr + 2); }
int bytearray_get(void* obj, std::size_t index) { if (!obj) return -1; auto* hdr = reinterpret_cast<std::size_t*>(obj); if (index >= hdr[0]) return -1; return static_cast<int>(bytearray_buf(obj)[index]); }
void bytearray_set(void* obj, std::size_t index, int value) {
  if (!obj) return; auto* hdr = reinterpret_cast<std::size_t*>(obj); if (index >= hdr[0]) return; bytearray_buf(obj)[index] = static_cast<unsigned char>(value & 0xFF);
}
void bytearray_append(void* obj, int value) {
  if (!obj) return; auto* hdr = reinterpret_cast<std::size_t*>(obj); auto* buf = bytearray_buf(obj); std::size_t len = hdr[0], cap = hdr[1];
  if (len + 1 > cap) {
    // grow: allocate new payload, copy, free old, re-link header
    std::size_t ncap = cap * 2;
    const std::size_t payloadSize = sizeof(ByteArrayPayload) + ncap;
    auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::ByteArray));
    auto* nhdr = reinterpret_cast<std::size_t*>(bytes);
    nhdr[0] = len; nhdr[1] = ncap;
    auto* nbuf = reinterpret_cast<unsigned char*>(nhdr + 2);
    std::memcpy(nbuf, buf, len);
    // replace header in-place: this object identity changes; for simplicity, we just mutate hdr fields and keep existing buffer if space permits
    // In this minimal runtime, allocate a new array object and leave old to be GC'd.
    // Copy back to caller by updating fields and pointer content is not possible without moving references; so we fallback to simple push when cap allows.
    // To avoid identity change, we conservatively cap appends to capacity in this subset.
    // If full, do nothing.
    free_obj(reinterpret_cast<ObjectHeader*>(bytes));
    return;
  }
  buf[len] = static_cast<unsigned char>(value & 0xFF); hdr[0] = len + 1;
}

// Filesystem helpers
void* os_getcwd() {
  char buf[4096];
#if defined(_WIN32)
  if (!_getcwd(buf, sizeof(buf))) return nullptr;
#else
  if (!::getcwd(buf, sizeof(buf))) return nullptr;
#endif
  return string_from_cstr(buf);
}
bool os_mkdir(const char* path, int mode) {
  if (!path) return false;
#if defined(_WIN32)
  (void)mode; return ::mkdir(path) == 0;
#else
  return ::mkdir(path, static_cast<mode_t>(mode)) == 0;
#endif
}
bool os_remove(const char* path) { if (!path) return false; return ::remove(path) == 0; }
bool os_rename(const char* src, const char* dst) { if (!src || !dst) return false; return ::rename(src, dst) == 0; }

// No module registry: imports are handled AOT in Codegen and linked statically.

// ---- Subprocess shims ----
static int32_t decode_exit_code(int rc) {
#if defined(__unix__) || defined(__APPLE__)
  if (rc == -1) return -1;
  // system() returns wait status; decode exit status when possible
  if (WIFEXITED(rc)) return static_cast<int32_t>(WEXITSTATUS(rc));
  return static_cast<int32_t>(rc);
#else
  return static_cast<int32_t>(rc);
#endif
}

int32_t subprocess_run(void* cmd) {
  const char* c = string_data(cmd);
  if (!c) return -1;
  int rc = std::system(c);
  return decode_exit_code(rc);
}
int32_t subprocess_call(void* cmd) { return subprocess_run(cmd); }
int32_t subprocess_check_call(void* cmd) {
  int32_t rc = subprocess_run(cmd);
  if (rc != 0) {
    rt_raise("CalledProcessError", "subprocess returned non-zero exit status");
  }
  return rc;
}

} // namespace pycc::rt

extern "C" int32_t pycc_subprocess_run(void* cmd) { return ::pycc::rt::subprocess_run(cmd); }
extern "C" int32_t pycc_subprocess_call(void* cmd) { return ::pycc::rt::subprocess_call(cmd); }
extern "C" int32_t pycc_subprocess_check_call(void* cmd) { return ::pycc::rt::subprocess_check_call(cmd); }

// ===== Concurrency scaffolding =====
namespace pycc::rt {

struct ThreadHandle {
  std::thread t;
  std::mutex mu;
  std::condition_variable cv;
  bool done{false};
  std::vector<unsigned char> retBuf;
};

struct Chan {
  std::mutex mu;
  std::condition_variable cv_not_empty;
  std::condition_variable cv_not_full;
  std::deque<void*> q;
  std::size_t cap{0};
  bool closed{false};
};

struct AtomicInt {
  std::atomic<long long> v{0};
};

RtThreadHandle* rt_spawn(RtStart fn, const void* payload, std::size_t len) {
  if (fn == nullptr) return nullptr;
  auto* h = new ThreadHandle();
  // Copy payload bytes
  std::vector<unsigned char> pay;
  if (payload && len > 0) { pay.assign(static_cast<const unsigned char*>(payload), static_cast<const unsigned char*>(payload) + len); }
  h->t = std::thread([h, fn, pay = std::move(pay)]() mutable {
    void* retPtr = nullptr;
    std::size_t retLen = 0;
    // Run entry
    fn(pay.empty() ? nullptr : pay.data(), pay.size(), &retPtr, &retLen);
    // Marshal return as bytes if provided
    if (retPtr != nullptr && retLen > 0) {
      h->retBuf.resize(retLen);
      std::memcpy(h->retBuf.data(), retPtr, retLen);
    }
    {
      std::lock_guard<std::mutex> lk(h->mu);
      h->done = true;
    }
    h->cv.notify_all();
  });
  return reinterpret_cast<RtThreadHandle*>(h);
}

bool rt_join(RtThreadHandle* handle, void** ret, std::size_t* ret_len) {
  if (!handle) return false;
  auto* h = reinterpret_cast<ThreadHandle*>(handle);
  {
    std::unique_lock<std::mutex> lk(h->mu);
    h->cv.wait(lk, [&]{ return h->done; });
  }
  if (h->t.joinable()) h->t.join();
  if (ret && ret_len) {
    if (!h->retBuf.empty()) {
      *ret_len = h->retBuf.size();
      void* buf = std::malloc(h->retBuf.size());
      if (buf) { std::memcpy(buf, h->retBuf.data(), h->retBuf.size()); *ret = buf; }
      else { *ret = nullptr; *ret_len = 0; }
    } else { *ret = nullptr; *ret_len = 0; }
  }
  return true;
}

void rt_thread_handle_destroy(RtThreadHandle* handle) {
  if (!handle) return; auto* h = reinterpret_cast<ThreadHandle*>(handle); delete h;
}

RtChannelHandle* chan_new(std::size_t capacity) { auto* ch = new Chan(); ch->cap = (capacity == 0 ? 1 : capacity); return reinterpret_cast<RtChannelHandle*>(ch); }
void chan_close(RtChannelHandle* handle) {
  if (!handle) return; auto* ch = reinterpret_cast<Chan*>(handle); std::lock_guard<std::mutex> lk(ch->mu); ch->closed = true; ch->cv_not_empty.notify_all(); ch->cv_not_full.notify_all();
}
void chan_send(RtChannelHandle* handle, void* value) {
  auto* ch = reinterpret_cast<Chan*>(handle); if (!ch) return;
  // Enforce cross-thread immutability/message-passing discipline: only immutable payloads (or nullptr).
  if (value != nullptr) {
    auto* header = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(value) - sizeof(ObjectHeader));
    const auto tag = static_cast<TypeTag>(header->tag);
    const bool immutable = (tag == TypeTag::String) || (tag == TypeTag::Int) || (tag == TypeTag::Float) || (tag == TypeTag::Bool) || (tag == TypeTag::Bytes);
    if (!immutable) {
      rt_raise("TypeError", "chan_send: only immutable payloads (int/float/bool/str/bytes) allowed across threads");
      return;
    }
  }
  std::unique_lock<std::mutex> lk(ch->mu);
  ch->cv_not_full.wait(lk, [&]{ return ch->closed || ch->q.size() < ch->cap; });
  if (ch->closed) return;
  ch->q.push_back(value);
  lk.unlock();
  ch->cv_not_empty.notify_one();
}
void* chan_recv(RtChannelHandle* handle) {
  auto* ch = reinterpret_cast<Chan*>(handle); if (!ch) return nullptr;
  std::unique_lock<std::mutex> lk(ch->mu);
  ch->cv_not_empty.wait(lk, [&]{ return ch->closed || !ch->q.empty(); });
  if (ch->q.empty()) return nullptr; // closed
  void* v = ch->q.front(); ch->q.pop_front();
  lk.unlock(); ch->cv_not_full.notify_one();
  return v;
}

RtAtomicIntHandle* atomic_int_new(long long initial) { auto* a = new AtomicInt(); a->v.store(initial, std::memory_order_relaxed); return reinterpret_cast<RtAtomicIntHandle*>(a); }
long long atomic_int_load(RtAtomicIntHandle* h) { if (!h) return 0; auto* a = reinterpret_cast<AtomicInt*>(h); return a->v.load(std::memory_order_acquire); }
void atomic_int_store(RtAtomicIntHandle* h, long long v) { if (!h) return; auto* a = reinterpret_cast<AtomicInt*>(h); a->v.store(v, std::memory_order_release); }
long long atomic_int_add_fetch(RtAtomicIntHandle* h, long long d) { if (!h) return 0; auto* a = reinterpret_cast<AtomicInt*>(h); return a->v.fetch_add(d, std::memory_order_acq_rel) + d; }

} // namespace pycc::rt

// ===== JSON shims =====
namespace pycc::rt {

static TypeTag type_of(void* obj) {
  if (!obj) return TypeTag::Object;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  return static_cast<TypeTag>(h->tag);
}

// removed: superseded by json_dump_str with UTF-8/ensure_ascii
static void indent_nl(std::string& out, int depth, int indent) {
  out.push_back('\n');
  for (int i=0;i<depth*indent;++i) out.push_back(' ');
}

struct DumpOpts { int indent{0}; bool ensureAscii{false}; const char* sepItem{nullptr}; const char* sepKv{nullptr}; bool sortKeys{false}; };

static void emit_codepoint_escaped(uint32_t cp, std::string& out) {
  auto emit_u4 = [&](uint16_t x){
    static const char* H="0123456789abcdef";
    out += "\\u";
    out.push_back(H[(x>>12)&0xF]); out.push_back(H[(x>>8)&0xF]); out.push_back(H[(x>>4)&0xF]); out.push_back(H[x&0xF]);
  };
  if (cp <= 0xFFFF) emit_u4(static_cast<uint16_t>(cp));
  else { uint32_t v = cp - 0x10000; uint16_t hi = 0xD800 | ((v>>10)&0x3FF); uint16_t lo = 0xDC00 | (v & 0x3FF); emit_u4(hi); emit_u4(lo); }
}

static void append_utf8_cp(uint32_t cp, std::string& o) {
  if (cp<=0x7F) o.push_back(static_cast<char>(cp));
  else if (cp<=0x7FF) { o.push_back(static_cast<char>(0xC0|(cp>>6))); o.push_back(static_cast<char>(0x80|(cp&0x3F))); }
  else if (cp<=0xFFFF) { o.push_back(static_cast<char>(0xE0|(cp>>12))); o.push_back(static_cast<char>(0x80|((cp>>6)&0x3F))); o.push_back(static_cast<char>(0x80|(cp&0x3F))); }
  else { o.push_back(static_cast<char>(0xF0|(cp>>18))); o.push_back(static_cast<char>(0x80|((cp>>12)&0x3F))); o.push_back(static_cast<char>(0x80|((cp>>6)&0x3F))); o.push_back(static_cast<char>(0x80|(cp&0x3F))); }
}

static void json_dump_str(const char* s, std::size_t n, std::string& out, const DumpOpts& opts) {
  out.push_back('"');
  for (std::size_t i = 0; i < n; ) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80U) {
      switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
          if (c < 0x20U) { char hex[3]; static const char* H="0123456789abcdef"; out += "\\u00"; hex[0]=H[(c>>4)&0xF]; hex[1]=H[c&0xF]; hex[2]='\0'; out += hex; }
          else out.push_back(static_cast<char>(c));
      }
      ++i;
    } else {
      // Decode UTF-8 to code point
      uint32_t cp = 0; int extra = 0;
      if ((c & 0xE0U) == 0xC0U) { cp = c & 0x1FU; extra = 1; }
      else if ((c & 0xF0U) == 0xE0U) { cp = c & 0x0FU; extra = 2; }
      else if ((c & 0xF8U) == 0xF0U) { cp = c & 0x07U; extra = 3; }
      else { // invalid start; emit as-is escaped
        if (opts.ensureAscii) emit_codepoint_escaped(c, out); else out.push_back(static_cast<char>(c)); ++i; continue;
      }
      if (i + extra >= n) { // truncated, emit bytes
        for (int k=0;k<=extra && i<n;++k,++i) { if (opts.ensureAscii) emit_codepoint_escaped(s[i], out); else out.push_back(s[i]); }
        continue;
      }
      ++i; for (int k=0;k<extra;++k,++i) { cp = (cp << 6U) | (static_cast<unsigned char>(s[i]) & 0x3FU); }
      if (opts.ensureAscii) emit_codepoint_escaped(cp, out); else append_utf8_cp(cp, out);
    }
  }
  out.push_back('"');
}

static void json_dumps_rec(void* obj, std::string& out, const DumpOpts& opts, int depth) {
  switch (type_of(obj)) {
    case TypeTag::String: {
      const char* d = string_data(obj); std::size_t n = string_len(obj); json_dump_str(d, n, out, opts); return;
    }
    case TypeTag::Int: {
      out += std::to_string(static_cast<long long>(box_int_value(obj))); return;
    }
    case TypeTag::Float: {
      char buf[64]; std::snprintf(buf, sizeof(buf), "%g", box_float_value(obj)); out += buf; return;
    }
    case TypeTag::Bool: { out += (box_bool_value(obj) ? "true" : "false"); return; }
    case TypeTag::List: {
      auto* meta = reinterpret_cast<std::size_t*>(obj); std::size_t len = meta[0]; auto** items = reinterpret_cast<void**>(meta + 2);
      out.push_back('[');
      if (len && opts.indent>0) indent_nl(out, depth+1, opts.indent);
      for (std::size_t i=0;i<len;++i) {
        if (i) {
          if (opts.indent>0) { out.push_back(','); indent_nl(out, depth+1, opts.indent); }
          else if (opts.sepItem) { out += opts.sepItem; }
          else { out.push_back(','); }
        }
        json_dumps_rec(items[i], out, opts, depth+1);
      }
      if (len && opts.indent>0) indent_nl(out, depth, opts.indent);
      out.push_back(']');
      return;
    }
    case TypeTag::Dict: {
      // Note: only string keys supported
      auto* base = reinterpret_cast<unsigned char*>(obj);
      auto* pm = reinterpret_cast<std::size_t*>(base);
      const std::size_t cap = pm[1];
      auto** keys = reinterpret_cast<void**>(pm + 2);
      auto** vals = keys + cap;
      out.push_back('{');
      bool first = true;
      if (opts.sortKeys) {
        std::vector<void*> klist; klist.reserve(cap);
        for (std::size_t i=0;i<cap;++i) if (keys[i] != nullptr) klist.push_back(keys[i]);
        std::sort(klist.begin(), klist.end(), [](void* a, void* b){ return std::strcmp(string_data(a), string_data(b)) < 0; });
        for (void* kk : klist) {
          // find value
          std::size_t idx = ptr_hash(kk) & (cap - 1);
          while (keys[idx] != nullptr && keys[idx] != kk) { idx = (idx + 1) & (cap - 1); }
          void* vv = (keys[idx] == kk) ? vals[idx] : nullptr;
          if (type_of(kk) != TypeTag::String) { rt_raise("TypeError", "json.dumps: dict keys must be str"); out.clear(); return; }
          if (!first) {
            if (opts.indent>0) { out.push_back(','); indent_nl(out, depth+1, opts.indent); }
            else if (opts.sepItem) { out += opts.sepItem; }
            else { out.push_back(','); }
          }
          first=false;
          const char* kd = string_data(kk); std::size_t kn = string_len(kk); json_dump_str(kd, kn, out, opts);
          if (opts.indent>0) { out.push_back(':'); out.push_back(' '); }
          else if (opts.sepKv) { out += opts.sepKv; }
          else { out.push_back(':'); }
          json_dumps_rec(vv, out, opts, depth+1);
        }
      } else {
        for (std::size_t i=0;i<cap;++i) {
          if (keys[i] != nullptr) {
            if (type_of(keys[i]) != TypeTag::String) { rt_raise("TypeError", "json.dumps: dict keys must be str"); out.clear(); return; }
            if (!first) {
              if (opts.indent>0) { out.push_back(','); indent_nl(out, depth+1, opts.indent); }
              else if (opts.sepItem) { out += opts.sepItem; }
              else { out.push_back(','); }
            }
            first=false;
            const char* kd = string_data(keys[i]); std::size_t kn = string_len(keys[i]); json_dump_str(kd, kn, out, opts);
            if (opts.indent>0) { out.push_back(':'); out.push_back(' '); }
            else if (opts.sepKv) { out += opts.sepKv; }
            else { out.push_back(':'); }
            json_dumps_rec(vals[i], out, opts, depth+1);
          }
        }
      }
      if (!first && opts.indent>0) indent_nl(out, depth, opts.indent);
      out.push_back('}');
      return;
    }
    case TypeTag::Bytes:
    case TypeTag::ByteArray:
    case TypeTag::Object:
    default:
      out += "null"; return;
  }
}

void* json_dumps(void* obj) {
  return json_dumps_ex(obj, 0);
}

void* json_dumps_ex(void* obj, int indent) {
  std::string out;
  if (indent < 0) indent = 0;
  DumpOpts opts; opts.indent = indent; opts.ensureAscii = false; opts.sepItem = nullptr; opts.sepKv = nullptr; opts.sortKeys = false;
  json_dumps_rec(obj, out, opts, 0);
  if (rt_has_exception()) return nullptr;
  return string_new(out.data(), out.size());
}

void* json_dumps_opts(void* obj, int ensure_ascii, int indent, const char* item_sep, const char* kv_sep, int sort_keys) {
  DumpOpts opts; opts.indent = (indent<0)?0:indent; opts.ensureAscii = (ensure_ascii!=0); opts.sepItem = item_sep; opts.sepKv = kv_sep; opts.sortKeys = (sort_keys!=0);
  std::string out; json_dumps_rec(obj, out, opts, 0);
  if (rt_has_exception()) return nullptr;
  return string_new(out.data(), out.size());
}

struct JsonParser {
  const char* s; std::size_t n; std::size_t i;
  explicit JsonParser(const char* s_, std::size_t n_) : s(s_), n(n_), i(0) {}
  void skipws(){ while(i<n && (s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t')) ++i; }
  bool consume(char c){ skipws(); if (i<n && s[i]==c) { ++i; return true; } return false; }
  bool match(const char* t){ skipws(); std::size_t j=0; while(t[j]&& i+j<n && s[i+j]==t[j]) ++j; if(!t[j]){ i+=j; return true;} return false; }
  void append_utf8(uint32_t cp, std::string& o){ if (cp<=0x7F) o.push_back(static_cast<char>(cp)); else if (cp<=0x7FF){ o.push_back(static_cast<char>(0xC0|(cp>>6))); o.push_back(static_cast<char>(0x80|(cp&0x3F))); } else if (cp<=0xFFFF){ o.push_back(static_cast<char>(0xE0|(cp>>12))); o.push_back(static_cast<char>(0x80|((cp>>6)&0x3F))); o.push_back(static_cast<char>(0x80|(cp&0x3F))); } else { o.push_back(static_cast<char>(0xF0|(cp>>18))); o.push_back(static_cast<char>(0x80|((cp>>12)&0x3F))); o.push_back(static_cast<char>(0x80|((cp>>6)&0x3F))); o.push_back(static_cast<char>(0x80|(cp&0x3F))); } }
  void* parseString(){
    auto hexval=[&](char c)->int{ if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return 10+(c-'a'); if(c>='A'&&c<='F') return 10+(c-'A'); return -1; };
    skipws(); if (i>=n || s[i] != '"') { rt_raise("ValueError","json: expected string"); return nullptr; } ++i; std::string out;
    while(i<n){ char c=s[i++]; if(c=='"') break; if(c=='\\'&& i<n){ char e=s[i++]; switch(e){ case '"': out.push_back('"'); break; case '\\': out.push_back('\\'); break; case '/': out.push_back('/'); break; case 'b': out.push_back('\b'); break; case 'f': out.push_back('\f'); break; case 'n': out.push_back('\n'); break; case 'r': out.push_back('\r'); break; case 't': out.push_back('\t'); break; case 'u': {
              if (i+4>n) { rt_raise("ValueError","json: invalid unicode escape"); return nullptr; }
              int h1=hexval(s[i]),h2=hexval(s[i+1]),h3=hexval(s[i+2]),h4=hexval(s[i+3]);
              if (h1<0||h2<0||h3<0||h4<0) { rt_raise("ValueError","json: invalid unicode escape"); return nullptr; }
              uint32_t cp = static_cast<uint32_t>((h1<<12)|(h2<<8)|(h3<<4)|h4);
              i+=4;
              if (cp>=0xD800 && cp<=0xDBFF) {
                if (!(i+6<=n && s[i]=='\\' && s[i+1]=='u')) { rt_raise("ValueError","json: invalid unicode surrogate"); return nullptr; }
                i+=2; int h5=hexval(s[i]),h6=hexval(s[i+1]),h7=hexval(s[i+2]),h8=hexval(s[i+3]);
                if (h5<0||h6<0||h7<0||h8<0) { rt_raise("ValueError","json: invalid unicode surrogate"); return nullptr; }
                uint32_t low = static_cast<uint32_t>((h5<<12)|(h6<<8)|(h7<<4)|h8); i+=4;
                if (!(low>=0xDC00 && low<=0xDFFF)) { rt_raise("ValueError","json: invalid unicode surrogate"); return nullptr; }
                cp = 0x10000 + (((cp-0xD800)<<10) | (low-0xDC00));
              }
              append_utf8(cp,out); break; }
            default: out.push_back(e); break; }
          } else { out.push_back(c);} }
    return string_new(out.data(), out.size());
  }
  void* parseNumber(){ skipws(); std::size_t start=i; if(i<n && (s[i]=='-'||s[i]=='+')) ++i; bool hasDot=false, hasExp=false; while(i<n){ char c=s[i]; if(c>='0'&&c<='9'){ ++i; continue; } if(c=='.'){ hasDot=true; ++i; continue; } if(c=='e'||c=='E'){ hasExp=true; ++i; if(i<n && (s[i]=='+'||s[i]=='-')) ++i; while(i<n && s[i]>='0'&&s[i]<='9') ++i; break;} break; }
  std::string t(s+start, i-start); if(hasDot||hasExp){ return box_float(std::strtod(t.c_str(), nullptr)); } else { long long v=std::strtoll(t.c_str(), nullptr, 10); return box_int(v);} }
  void* parseValue(){ skipws(); if(i>=n){ rt_raise("ValueError","json: unexpected end"); return nullptr;} char c=s[i]; if(c=='"') return parseString(); if(c=='{' ) return parseObject(); if(c=='[') return parseArray(); if(c=='t'){ if(match("true")) return box_bool(true);} if(c=='f'){ if(match("false")) return box_bool(false);} if(c=='n'){ if(match("null")) return nullptr;} return parseNumber(); }
  void* parseArray(){ if(!consume('[')){ rt_raise("ValueError","json: expected '['"); return nullptr;} void* list=nullptr; list = list_new(4); while(true){ skipws(); if(consume(']')) break; void* v = parseValue(); if(rt_has_exception()) return nullptr; list_push_slot(&list, v); skipws(); if(consume(']')) break; if(!consume(',')){ rt_raise("ValueError","json: expected ',' or ']'"); return nullptr; } }
  return list; }
  void* parseObject(){ if(!consume('{')){ rt_raise("ValueError","json: expected '{'"); return nullptr;} void* d = dict_new(8); while(true){ skipws(); if(consume('}')) break; void* k = parseString(); if(rt_has_exception()) return nullptr; skipws(); if(!consume(':')){ rt_raise("ValueError","json: expected ':'"); return nullptr; } void* v = parseValue(); if(rt_has_exception()) return nullptr; dict_set(&d, k, v); skipws(); if(consume('}')) break; if(!consume(',')){ rt_raise("ValueError","json: expected ',' or '}'"); return nullptr; } }
  return d; }
};

void* json_loads(void* s) {
  const char* d = string_data(s); std::size_t n = string_len(s);
  JsonParser p(d, n);
  void* v = p.parseValue();
  return v;
}

// ---------------------------
// Itertools (materialized list-based)
// ---------------------------
static inline void* rt_make_list2(void* a, void* b) {
  void* pair = list_new(2);
  list_push_slot(&pair, a);
  list_push_slot(&pair, b);
  return pair;
}

void* itertools_chain2(void* a, void* b) {
  if (a == nullptr && b == nullptr) return list_new(0);
  std::size_t la = (a ? list_len(a) : 0);
  std::size_t lb = (b ? list_len(b) : 0);
  void* out = list_new(la + lb);
  for (std::size_t i = 0; i < la; ++i) { list_push_slot(&out, list_get(a, i)); }
  for (std::size_t i = 0; i < lb; ++i) { list_push_slot(&out, list_get(b, i)); }
  return out;
}

void* itertools_chain_from_iterable(void* list_of_lists) {
  void* out = list_new(0);
  if (list_of_lists == nullptr) return out;
  const std::size_t n = list_len(list_of_lists);
  for (std::size_t i = 0; i < n; ++i) {
    void* sub = list_get(list_of_lists, i);
    if (sub == nullptr) continue;
    const std::size_t m = list_len(sub);
    for (std::size_t j = 0; j < m; ++j) { list_push_slot(&out, list_get(sub, j)); }
  }
  return out;
}

void* itertools_product2(void* a, void* b) {
  void* out = list_new(0);
  const std::size_t la = (a ? list_len(a) : 0);
  const std::size_t lb = (b ? list_len(b) : 0);
  for (std::size_t i = 0; i < la; ++i) {
    void* ai = list_get(a, i);
    for (std::size_t j = 0; j < lb; ++j) {
      void* bj = list_get(b, j);
      void* pair = rt_make_list2(ai, bj);
      list_push_slot(&out, pair);
    }
  }
  return out;
}

static void rt_permute_rec(void* src, std::vector<int>& idx, int r, int depth, std::vector<int>& used, void*& out) {
  if (depth == r) {
    void* tup = list_new(static_cast<std::size_t>(r));
    for (int k = 0; k < r; ++k) { list_push_slot(&tup, list_get(src, static_cast<std::size_t>(idx[k]))); }
    list_push_slot(&out, tup);
    return;
  }
  const std::size_t n = list_len(src);
  for (std::size_t i = 0; i < n; ++i) {
    if (used[static_cast<std::size_t>(i)]) continue;
    used[static_cast<std::size_t>(i)] = 1;
    idx[depth] = static_cast<int>(i);
    rt_permute_rec(src, idx, r, depth + 1, used, out);
    used[static_cast<std::size_t>(i)] = 0;
  }
}

void* itertools_permutations(void* a, int r) {
  if (a == nullptr) return list_new(0);
  const std::size_t n = list_len(a);
  int rr = (r <= 0) ? static_cast<int>(n) : r;
  if (rr < 0 || static_cast<std::size_t>(rr) > n) return list_new(0);
  void* out = list_new(0);
  std::vector<int> idx(static_cast<std::size_t>(rr), 0);
  std::vector<int> used(n, 0);
  rt_permute_rec(a, idx, rr, 0, used, out);
  return out;
}

static void rt_comb_rec(void* src, int r, int start, std::vector<int>& cur, void*& out, bool with_replacement) {
  if (static_cast<int>(cur.size()) == r) {
    void* tup = list_new(static_cast<std::size_t>(r));
    for (int id : cur) { list_push_slot(&tup, list_get(src, static_cast<std::size_t>(id))); }
    list_push_slot(&out, tup);
    return;
  }
  const std::size_t n = list_len(src);
  for (int i = start; i < static_cast<int>(n); ++i) {
    cur.push_back(i);
    rt_comb_rec(src, r, with_replacement ? i : (i + 1), cur, out, with_replacement);
    cur.pop_back();
  }
}

void* itertools_combinations(void* a, int r) {
  if (a == nullptr || r <= 0) return list_new(0);
  void* out = list_new(0);
  std::vector<int> cur;
  rt_comb_rec(a, r, 0, cur, out, false);
  return out;
}

void* itertools_combinations_with_replacement(void* a, int r) {
  if (a == nullptr || r <= 0) return list_new(0);
  void* out = list_new(0);
  std::vector<int> cur;
  rt_comb_rec(a, r, 0, cur, out, true);
  return out;
}

void* itertools_zip_longest2(void* a, void* b, void* fillvalue) {
  void* out = list_new(0);
  const std::size_t la = (a ? list_len(a) : 0);
  const std::size_t lb = (b ? list_len(b) : 0);
  const std::size_t lm = (la > lb) ? la : lb;
  for (std::size_t i = 0; i < lm; ++i) {
    void* ai = (i < la) ? list_get(a, i) : fillvalue;
    void* bi = (i < lb) ? list_get(b, i) : fillvalue;
    void* pair = rt_make_list2(ai, bi);
    list_push_slot(&out, pair);
  }
  return out;
}

void* itertools_islice(void* a, int start, int stop, int step) {
  if (a == nullptr) return list_new(0);
  if (step == 0) step = 1;
  const std::size_t n = list_len(a);
  int s = (start < 0) ? 0 : start;
  int e = (stop < 0 || stop > static_cast<int>(n)) ? static_cast<int>(n) : stop;
  if (s > e) return list_new(0);
  void* out = list_new(0);
  for (int i = s; i < e; i += step) { list_push_slot(&out, list_get(a, static_cast<std::size_t>(i))); }
  return out;
}

void* itertools_accumulate_sum(void* a) {
  if (a == nullptr) return list_new(0);
  const std::size_t n = list_len(a);
  void* out = list_new(0);
  bool useFloat = false;
  if (n > 0) {
    double d0 = box_float_value(list_get(a, 0));
    long long i0 = box_int_value(list_get(a, 0));
    useFloat = (d0 != static_cast<double>(i0));
  }
  double fsum = 0.0; long long isum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (useFloat) {
      fsum += box_float_value(list_get(a, i));
      list_push_slot(&out, box_float(fsum));
    } else {
      isum += box_int_value(list_get(a, i));
      list_push_slot(&out, box_int(isum));
    }
  }
  return out;
}

void* itertools_repeat(void* obj, int times) {
  if (times <= 0) return list_new(0);
  void* out = list_new(static_cast<std::size_t>(times));
  for (int i = 0; i < times; ++i) { list_push_slot(&out, obj); }
  return out;
}

void* itertools_pairwise(void* a) {
  if (a == nullptr) return list_new(0);
  const std::size_t n = list_len(a);
  if (n < 2) return list_new(0);
  void* out = list_new(0);
  for (std::size_t i = 0; i + 1 < n; ++i) {
    void* pair = rt_make_list2(list_get(a, i), list_get(a, i + 1));
    list_push_slot(&out, pair);
  }
  return out;
}

void* itertools_batched(void* a, int n) {
  if (a == nullptr || n <= 0) return list_new(0);
  const std::size_t len = list_len(a);
  void* out = list_new(0);
  for (std::size_t i = 0; i < len; i += static_cast<std::size_t>(n)) {
    std::size_t end = i + static_cast<std::size_t>(n);
    if (end > len) end = len;
    void* batch = list_new(static_cast<std::size_t>(n));
    for (std::size_t j = i; j < end; ++j) { list_push_slot(&batch, list_get(a, j)); }
    list_push_slot(&out, batch);
  }
  return out;
}

void* itertools_compress(void* data, void* selectors) {
  if (data == nullptr || selectors == nullptr) return list_new(0);
  const std::size_t nd = list_len(data);
  const std::size_t ns = list_len(selectors);
  const std::size_t n = (nd < ns) ? nd : ns;
  void* out = list_new(0);
  for (std::size_t i = 0; i < n; ++i) {
    void* sel = list_get(selectors, i);
    bool truth = box_bool_value(sel);
    if (truth) { list_push_slot(&out, list_get(data, i)); }
  }
  return out;
}

} // namespace pycc::rt

extern "C" void* pycc_json_dumps(void* obj) { return ::pycc::rt::json_dumps(obj); }
extern "C" void* pycc_json_loads(void* s) { return ::pycc::rt::json_loads(s); }
extern "C" void* pycc_json_dumps_ex(void* obj, int indent) { return ::pycc::rt::json_dumps_ex(obj, indent); }
extern "C" void* pycc_json_dumps_opts(void* obj, int ensure_ascii, int indent, const char* item_sep, const char* kv_sep, int sort_keys) { return ::pycc::rt::json_dumps_opts(obj, ensure_ascii, indent, item_sep, kv_sep, sort_keys); }

extern "C" double pycc_time_time() { return ::pycc::rt::time_time(); }
extern "C" long long pycc_time_time_ns() { return static_cast<long long>(::pycc::rt::time_time_ns()); }
extern "C" double pycc_time_monotonic() { return ::pycc::rt::time_monotonic(); }
extern "C" long long pycc_time_monotonic_ns() { return static_cast<long long>(::pycc::rt::time_monotonic_ns()); }
extern "C" double pycc_time_perf_counter() { return ::pycc::rt::time_perf_counter(); }
extern "C" long long pycc_time_perf_counter_ns() { return static_cast<long long>(::pycc::rt::time_perf_counter_ns()); }
extern "C" double pycc_time_process_time() { return ::pycc::rt::time_process_time(); }
extern "C" void pycc_time_sleep(double s) { ::pycc::rt::time_sleep(s); }

extern "C" void* pycc_datetime_now() { return ::pycc::rt::datetime_now(); }
extern "C" void* pycc_datetime_utcnow() { return ::pycc::rt::datetime_utcnow(); }
extern "C" void* pycc_datetime_fromtimestamp(double ts) { return ::pycc::rt::datetime_fromtimestamp(ts); }
extern "C" void* pycc_datetime_utcfromtimestamp(double ts) { return ::pycc::rt::datetime_utcfromtimestamp(ts); }

// C ABI exports for itertools
extern "C" void* pycc_itertools_chain2(void* a, void* b) { return ::pycc::rt::itertools_chain2(a,b); }
extern "C" void* pycc_itertools_chain_from_iterable(void* x) { return ::pycc::rt::itertools_chain_from_iterable(x); }
extern "C" void* pycc_itertools_product2(void* a, void* b) { return ::pycc::rt::itertools_product2(a,b); }
extern "C" void* pycc_itertools_permutations(void* a, int r) { return ::pycc::rt::itertools_permutations(a,r); }
extern "C" void* pycc_itertools_combinations(void* a, int r) { return ::pycc::rt::itertools_combinations(a,r); }
extern "C" void* pycc_itertools_combinations_with_replacement(void* a, int r) { return ::pycc::rt::itertools_combinations_with_replacement(a,r); }
extern "C" void* pycc_itertools_zip_longest2(void* a, void* b, void* fill) { return ::pycc::rt::itertools_zip_longest2(a,b,fill); }
extern "C" void* pycc_itertools_islice(void* a, int start, int stop, int step) { return ::pycc::rt::itertools_islice(a,start,stop,step); }
extern "C" void* pycc_itertools_accumulate_sum(void* a) { return ::pycc::rt::itertools_accumulate_sum(a); }
extern "C" void* pycc_itertools_repeat(void* obj, int times) { return ::pycc::rt::itertools_repeat(obj,times); }
extern "C" void* pycc_itertools_pairwise(void* a) { return ::pycc::rt::itertools_pairwise(a); }
extern "C" void* pycc_itertools_batched(void* a, int n) { return ::pycc::rt::itertools_batched(a,n); }
extern "C" void* pycc_itertools_compress(void* a, void* b) { return ::pycc::rt::itertools_compress(a,b); }
