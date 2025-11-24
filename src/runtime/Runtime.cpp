/***
 * Name: pycc::rt (Runtime impl)
 * Purpose: Minimal precise mark-sweep GC and string objects.
 */
#include "runtime/All.h"
#include "runtime/detail/JsonTypes.h"
#include "runtime/detail/JsonHandlers.h"
#include "runtime/detail/RuntimeIntrospection.h"
#include "runtime/detail/Base64Handlers.h"
#include "runtime/detail/EncodingHandlers.h"
#include "runtime/detail/HtmlHandlers.h"
#include "runtime/detail/StructHandlers.h"
#include "runtime/detail/ArgparseHandlers.h"
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
#include <unordered_map>
#include <vector>
#include <array>
#include <string_view>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#include <sys/utsname.h>
#endif
#include <unistd.h>
#include <filesystem>
#include <system_error>
#include <cmath>
#include <random>

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
// Forward declare mark for helper calls
static void mark(ObjectHeader* header);

// Per-tag mark helpers to keep switch shallow
static inline void mark_list_body(ObjectHeader* header) {
  auto* base = reinterpret_cast<unsigned char*>(header);
  auto* payload = reinterpret_cast<std::size_t*>(base + sizeof(ObjectHeader));
  const std::size_t len = payload[0];
  auto* const* items = reinterpret_cast<void* const*>(payload + 2);
  for (std::size_t i = 0; i < len; ++i) {
    const void* valuePtr = items[i];
    if (!valuePtr) continue;
    if (ObjectHeader* headerPtr = find_object_for_pointer(valuePtr)) { mark(headerPtr); }
  }
}

static inline void mark_object_body(ObjectHeader* header) {
  auto* base = reinterpret_cast<unsigned char*>(header);
  auto* payload = reinterpret_cast<std::size_t*>(base + sizeof(ObjectHeader));
  const std::size_t fields = payload[0];
  auto* const* values = reinterpret_cast<void* const*>(payload + 1);
  for (std::size_t i = 0; i < fields; ++i) {
    const void* valuePtr = values[i];
    if (!valuePtr) continue;
    if (ObjectHeader* headerPtr = find_object_for_pointer(valuePtr)) { mark(headerPtr); }
  }
  const void* attrDictPtr = values[fields];
  if (attrDictPtr != nullptr) {
    if (ObjectHeader* ad = find_object_for_pointer(attrDictPtr)) { mark(ad); }
  }
}

static inline void mark_dict_body(ObjectHeader* header) {
  auto* base = reinterpret_cast<unsigned char*>(header);
  auto* payload = reinterpret_cast<std::size_t*>(base + sizeof(ObjectHeader));
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
}

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
    case TypeTag::List: mark_list_body(header); break;
    case TypeTag::Object: mark_object_body(header); break;
    case TypeTag::Dict: mark_dict_body(header); break;
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
extern "C" void* pycc_rt_exception_cause(void* exc) { return rt_exception_cause(exc); }
extern "C" void pycc_rt_exception_set_cause(void* exc, void* cause) { rt_exception_set_cause(exc, cause); }
extern "C" void* pycc_rt_exception_context(void* exc) { return rt_exception_context(exc); }
extern "C" void pycc_rt_exception_set_context(void* exc, void* ctx) { rt_exception_set_context(exc, ctx); }

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
  // Quiesce background GC and reset internal state for deterministic tests
  g_bg_enabled.store(false, std::memory_order_relaxed);
  g_barrier_mode.store(0, std::memory_order_relaxed);
  {
    const std::lock_guard<std::mutex> remLock(g_rem_mu);
    g_remembered.clear();
  }
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

// C ABI wrapper for string concatenation
extern "C" void* pycc_string_concat(void* a, void* b) { return ::pycc::rt::string_concat(a, b); }

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
  const std::size_t payloadSize = (sizeof(std::size_t) * 3) + (capacity * sizeof(void*)) + (capacity * sizeof(void*));
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Dict));
  auto* meta = reinterpret_cast<std::size_t*>(bytes);
  meta[0] = 0; meta[1] = capacity; meta[2] = 0; // len, cap, ver
  auto** keys = reinterpret_cast<void**>(meta + 3);
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
  auto** keys = reinterpret_cast<void**>(meta + 3);
  auto** vals = keys + cap;
  auto* bytes = static_cast<unsigned char*>(alloc_raw((sizeof(std::size_t) * 3) + (newCap * sizeof(void*)) + (newCap * sizeof(void*)), TypeTag::Dict));
  auto* nmeta = reinterpret_cast<std::size_t*>(bytes);
  nmeta[0] = 0; nmeta[1] = newCap; nmeta[2] = meta[2] + 1; // bump version on rehash
  auto** nkeys = reinterpret_cast<void**>(nmeta + 3);
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
  auto** keys = reinterpret_cast<void**>(meta + 3);
  auto** vals = keys + cap;
  // grow if load factor > 0.7 (cap should be power of two for masking)
  if ((len + 1) * 10 > cap * 7) { dict_rehash(dict_slot, cap * 2); meta = reinterpret_cast<std::size_t*>(*dict_slot); len = meta[0]; }
  const std::size_t ncap = meta[1]; keys = reinterpret_cast<void**>(meta + 3); vals = keys + ncap;
  std::size_t idx = ptr_hash(key) & (ncap - 1);
  while (true) {
    if (keys[idx] == nullptr || keys[idx] == key) {
      if (keys[idx] == nullptr) { meta[0] = len + 1; }
      gc_pre_barrier(&keys[idx]); keys[idx] = key; gc_write_barrier(&keys[idx], key);
      gc_pre_barrier(&vals[idx]); vals[idx] = value; gc_write_barrier(&vals[idx], value);
      // bump version on any set (insert or update)
      meta[2] = meta[2] + 1;
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
  auto** keys = reinterpret_cast<void**>(meta + 3);
  auto** vals = keys + cap;
  // Fast path: pointer-identity lookup via open addressing
  std::size_t idx = ptr_hash(key) & (cap - 1);
  for (;;) {
    void* k = keys[idx];
    if (k == nullptr) { break; }
    if (k == key) { return vals[idx]; }
    idx = (idx + 1) & (cap - 1);
  }
  // Fallback for string keys: allow lookup by string contents to interoperate with fresh string objects
  // Inspect header tag to detect string keys without relying on later helper
  bool key_is_string = false;
  if (key != nullptr) {
    auto* hk = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(key) - sizeof(ObjectHeader));
    key_is_string = (static_cast<TypeTag>(hk->tag) == TypeTag::String);
  }
  if (key_is_string) {
    for (std::size_t i = 0; i < cap; ++i) {
      void* kk = keys[i];
      if (kk == nullptr) { continue; }
      auto* hk2 = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(kk) - sizeof(ObjectHeader));
      if (static_cast<TypeTag>(hk2->tag) == TypeTag::String && string_eq(kk, key)) { return vals[i]; }
    }
  }
  return nullptr;
}

std::size_t dict_len(void* dict) {
  if (dict == nullptr) { return 0; }
  auto* meta = reinterpret_cast<std::size_t*>(dict);
  return meta[0];
}

extern "C" void* pycc_dict_iter_new(void* dict) {
  // Build a stable snapshot list of keys. We avoid holding runtime-global locks while allocating.
  // If the dict mutates during snapshot, retry a few times to capture a consistent view.
  void* snap = list_new(8);
  if (dict != nullptr) {
    for (int attempt = 0; attempt < 3; ++attempt) {
      // Create a fresh list for this attempt
      snap = list_new(8);
      auto* meta = reinterpret_cast<std::size_t*>(dict);
      const std::size_t cap = meta[1];
      const std::size_t ver = meta[2];
      auto** keys = reinterpret_cast<void**>(meta + 3);
      for (std::size_t i = 0; i < cap; ++i) {
        void* k = keys[i];
        if (k != nullptr) { list_push_slot(&snap, k); }
      }
      // Verify no mutation occurred during snapshot
      auto* meta2 = reinterpret_cast<std::size_t*>(dict);
      if (meta2[2] == ver) { break; }
      // else retry
    }
  }
  // Iterator layout: values[0] = snapshot list ptr, values[1] = next index (boxed int)
  void* it = object_new(2);
  auto* meta_it = reinterpret_cast<std::size_t*>(it);
  auto** vals_it = reinterpret_cast<void**>(meta_it + 1);
  gc_pre_barrier(&vals_it[0]); vals_it[0] = snap; gc_write_barrier(&vals_it[0], snap);
  void* zero = box_int(0);
  gc_pre_barrier(&vals_it[1]); vals_it[1] = zero; gc_write_barrier(&vals_it[1], zero);
  return it;
}

extern "C" void* pycc_dict_iter_next(void* it) {
  if (it == nullptr) { return nullptr; }
  // Do not hold g_mu while allocating box_int; iterate over a stable snapshot list.
  auto* meta_it = reinterpret_cast<std::size_t*>(it);
  auto** vals_it = reinterpret_cast<void**>(meta_it + 1);
  void* snap = vals_it[0]; if (snap == nullptr) return nullptr;
  const std::size_t n = list_len(snap);
  std::size_t idx = static_cast<std::size_t>(box_int_value(vals_it[1]));
  if (idx >= n) {
    void* endIdx = box_int(static_cast<int64_t>(n));
    gc_pre_barrier(&vals_it[1]);
    vals_it[1] = endIdx;
    gc_write_barrier(&vals_it[1], endIdx);
    return nullptr;
  }
  void* k = list_get(snap, static_cast<int64_t>(idx));
  void* nextIdx = box_int(static_cast<int64_t>(idx + 1));
  gc_pre_barrier(&vals_it[1]);
  vals_it[1] = nextIdx;
  gc_write_barrier(&vals_it[1], nextIdx);
  return k;
}

// C++ API wrappers to match header declarations
void* dict_iter_new(void* dict) { return pycc_dict_iter_new(dict); }
void* dict_iter_next(void* it) { return pycc_dict_iter_next(it); }

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
  auto** slot = object_attrs_slot(obj);
  // Ensure attribute dict exists under g_mu, but perform dict_set without holding it to avoid re-entrant deadlock
  {
    const std::lock_guard<std::mutex> lock(g_mu);
    if (*slot == nullptr) {
      // lazily create dict
      void* d = dict_new_locked(8);
      gc_pre_barrier(slot);
      gc_write_barrier(slot, d);
      *slot = d;
    }
  }
  // set into dict (dict_set acquires g_mu internally)
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
  // Allocate exception components; allocation helpers manage their own locking
  void* t = string_from_cstr(type_name);
  void* m = string_from_cstr(message);
  // Reserve slots: [0]=type, [1]=message, [2]=cause, [3]=context
  void* exc = object_new(4);
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

void rt_exception_set_cause(void* exc, void* cause_exc) {
  if (exc == nullptr) { return; }
  // If older exception objects exist with only 2 fields, ignore gracefully.
  if (object_field_count(exc) < 3) { return; }
  object_set(exc, 2, cause_exc);
}

void* rt_exception_cause(void* exc) {
  if (exc == nullptr) { return nullptr; }
  if (object_field_count(exc) < 3) { return nullptr; }
  return object_get(exc, 2);
}

void rt_exception_set_context(void* exc, void* ctx_exc) {
  if (exc == nullptr) { return; }
  if (object_field_count(exc) < 4) { return; }
  object_set(exc, 3, ctx_exc);
}

void* rt_exception_context(void* exc) {
  if (exc == nullptr) { return nullptr; }
  if (object_field_count(exc) < 4) { return nullptr; }
  return object_get(exc, 3);
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

// C ABI: expose bytes length for codegen len(bytes)
extern "C" uint64_t pycc_bytes_len(void* b) { return static_cast<uint64_t>(bytes_len(b)); }
extern "C" void* pycc_bytes_new(const void* data, uint64_t len) { return bytes_new(data, static_cast<std::size_t>(len)); }

int64_t bytes_find(void* haystack, void* needle) {
  if (!haystack || !needle) return -1;
  const unsigned char* h = bytes_data(haystack);
  const unsigned char* n = bytes_data(needle);
  const std::size_t lh = bytes_len(haystack), ln = bytes_len(needle);
  if (ln == 0) return 0;
  if (ln > lh) return -1;
  for (std::size_t i = 0; i + ln <= lh; ++i) {
    if (std::memcmp(h + i, n, ln) == 0) return static_cast<int64_t>(i);
  }
  return -1;
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
  const std::size_t nb = bytes_len(b);
  const unsigned char* p = bytes_data(b);
  std::string out;
  if (std::strcmp(enc, "utf-8") == 0) {
    if (!detail::decode_utf8_bytes(p, nb, err, out)) { rt_raise("UnicodeDecodeError", "invalid utf-8"); return nullptr; }
    return string_new(out.data(), out.size());
  }
  if (std::strcmp(enc, "ascii") == 0) {
    if (!detail::decode_ascii_bytes(p, nb, err, out)) { rt_raise("UnicodeDecodeError", "ascii codec can't decode byte"); return nullptr; }
    return string_new(out.data(), out.size());
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
  // ASCII-only fallback: map A-Z to a-z; leave other bytes unchanged
  const char* data = string_data(s);
  const std::size_t n = string_len(s);
  std::string out; out.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    unsigned char c = static_cast<unsigned char>(data[i]);
    if (c >= 'A' && c <= 'Z') out[i] = static_cast<char>(c - 'A' + 'a');
    else out[i] = static_cast<char>(c);
  }
  return string_new(out.data(), out.size());
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
    // In this minimal runtime, do not grow beyond capacity; ignore append when full.
    free_obj(reinterpret_cast<ObjectHeader*>(bytes));
    return;
  }
  buf[len] = static_cast<unsigned char>(value & 0xFF); hdr[0] = len + 1;
}

void bytearray_extend_from_bytes(void* obj, void* bytes) {
  if (!obj || !bytes) return;
  auto* hdr = reinterpret_cast<std::size_t*>(obj);
  auto* buf = bytearray_buf(obj);
  std::size_t len = hdr[0], cap = hdr[1];
  const unsigned char* src = bytes_data(bytes);
  std::size_t n = bytes_len(bytes);
  std::size_t can = (len + n <= cap) ? n : (cap > len ? (cap - len) : 0);
  if (can == 0) return;
  std::memcpy(buf + len, src, can);
  hdr[0] = len + can;
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

// C ABI wrappers for os module
extern "C" void* pycc_os_getcwd() { return ::pycc::rt::os_getcwd(); }
extern "C" int  pycc_os_mkdir(void* pathStr, int mode) {
  const char* p = ::pycc::rt::string_data(pathStr);
  return ::pycc::rt::os_mkdir(p, mode) ? 1 : 0;
}
extern "C" int  pycc_os_remove(void* pathStr) {
  const char* p = ::pycc::rt::string_data(pathStr);
  return ::pycc::rt::os_remove(p) ? 1 : 0;
}
extern "C" int  pycc_os_rename(void* a, void* b) {
  const char* sa = ::pycc::rt::string_data(a);
  const char* sb = ::pycc::rt::string_data(b);
  return ::pycc::rt::os_rename(sa, sb) ? 1 : 0;
}
extern "C" void* pycc_os_getenv(void* nameStr) {
  const char* n = ::pycc::rt::string_data(nameStr);
  return ::pycc::rt::os_getenv(n);
}

// ---- pathlib shims (std::filesystem based) ----
namespace {
static std::filesystem::path fs_from_rt(void* s) {
  if (!s) return {};
  const char* data = string_data(s);
  const std::size_t len = string_len(s);
  std::string sv(data ? data : "", data ? len : 0);
  std::u8string u8(reinterpret_cast<const char8_t*>(sv.data()), reinterpret_cast<const char8_t*>(sv.data()) + sv.size());
  return std::filesystem::path(u8);
}
static void* rt_from_fs(const std::filesystem::path& p) {
  auto u8 = p.generic_u8string();
  const char* bytes = reinterpret_cast<const char*>(u8.c_str());
  return string_new(bytes, u8.size());
}
static void* rt_from_str(const std::string& s) { return string_new(s.data(), s.size()); }
static bool fs_exists_nothrow(const std::filesystem::path& p) { std::error_code ec; return std::filesystem::exists(p, ec); }
static bool fs_is_regular_nothrow(const std::filesystem::path& p) { std::error_code ec; return std::filesystem::is_regular_file(p, ec); }
static bool fs_is_dir_nothrow(const std::filesystem::path& p) { std::error_code ec; return std::filesystem::is_directory(p, ec); }
static std::filesystem::path fs_abs_nothrow(const std::filesystem::path& p) { std::error_code ec; auto r = std::filesystem::absolute(p, ec); return ec ? p : r; }
static std::filesystem::path fs_resolve_nothrow(const std::filesystem::path& p) {
  std::error_code ec;
  auto r = std::filesystem::weakly_canonical(p, ec);
  if (!ec && !r.empty()) return r;
  // Fallback: absolute
  return fs_abs_nothrow(p);
}
static std::string detect_home() {
#if defined(_WIN32)
  if (const char* p = std::getenv("USERPROFILE")) { return std::string(p); }
  const char* drive = std::getenv("HOMEDRIVE");
  const char* path = std::getenv("HOMEPATH");
  if (drive && path) { return std::string(drive) + std::string(path); }
#else
  if (const char* p = std::getenv("HOME")) { return std::string(p); }
#endif
  // Fallback to cwd
  std::error_code ec; auto cwd = std::filesystem::current_path(ec);
  return ec ? std::string(".") : cwd.generic_string();
}
static bool wildcard_match(const std::string& name, const std::string& pattern) {
  // Simple glob: '*' matches any sequence, '?' matches single char (no character classes)
  size_t n = name.size(), m = pattern.size();
  size_t i = 0, j = 0, star = std::string::npos, mark = 0;
  while (i < n) {
    if (j < m && (pattern[j] == '?' || pattern[j] == name[i])) { ++i; ++j; continue; }
    if (j < m && pattern[j] == '*') { star = j++; mark = i; continue; }
    if (star != std::string::npos) { j = star + 1; i = ++mark; continue; }
    return false;
  }
  while (j < m && pattern[j] == '*') ++j;
  return j == m;
}
} // namespace

void* pathlib_cwd() {
  std::error_code ec; auto p = std::filesystem::current_path(ec);
  if (ec) return string_from_cstr("");
  return rt_from_fs(p);
}
void* pathlib_home() {
  return rt_from_str(detect_home());
}
void* pathlib_join2(void* a, void* b) {
  auto pa = fs_from_rt(a); auto pb = fs_from_rt(b);
  return rt_from_fs(pa / pb);
}
void* pathlib_parent(void* p) {
  return rt_from_fs(fs_from_rt(p).parent_path());
}
void* pathlib_basename(void* p) {
  return rt_from_fs(fs_from_rt(p).filename());
}
void* pathlib_suffix(void* p) { return rt_from_fs(fs_from_rt(p).extension()); }
void* pathlib_stem(void* p) { return rt_from_fs(fs_from_rt(p).stem()); }
void* pathlib_with_name(void* p, void* name) {
  auto base = fs_from_rt(p); auto nm = fs_from_rt(name);
  return rt_from_fs(base.parent_path() / nm);
}
void* pathlib_with_suffix(void* p, void* suffix) {
  auto base = fs_from_rt(p); auto s = fs_from_rt(suffix);
  std::filesystem::path out = base; out.replace_extension(s);
  return rt_from_fs(out);
}
void* pathlib_as_posix(void* p) { return rt_from_fs(fs_from_rt(p)); }
void* pathlib_as_uri(void* p) {
  auto abs = fs_resolve_nothrow(fs_from_rt(p));
  if (abs.empty() || !abs.is_absolute()) return string_from_cstr("");
  auto gen = abs.generic_u8string();
#if defined(_WIN32)
  // file:///C:/path
  std::string uri = std::string("file:///") + std::string(reinterpret_cast<const char*>(gen.c_str()), gen.size());
#else
  std::string uri = std::string("file://") + std::string(reinterpret_cast<const char*>(gen.c_str()), gen.size());
#endif
  return string_new(uri.data(), uri.size());
}
void* pathlib_resolve(void* p) { return rt_from_fs(fs_resolve_nothrow(fs_from_rt(p))); }
void* pathlib_absolute(void* p) { return rt_from_fs(fs_abs_nothrow(fs_from_rt(p))); }
void* pathlib_parts(void* p) {
  auto path = fs_from_rt(p);
  // Build list of parts in generic form
  void* lst = list_new(4);
  void** slot = &lst;
  for (const auto& part : path) {
    auto u8 = part.generic_u8string();
    void* s = string_new(reinterpret_cast<const char*>(u8.c_str()), u8.size());
    list_push_slot(slot, s);
  }
  return lst;
}
bool pathlib_match(void* p, void* pattern) {
  auto name = fs_from_rt(p).filename().generic_u8string();
  std::string nm(reinterpret_cast<const char*>(name.c_str()), name.size());
  const char* pat = string_data(pattern); std::size_t len = string_len(pattern);
  std::string patS(pat ? pat : "", pat ? len : 0);
  return wildcard_match(nm, patS);
}
bool pathlib_exists(void* p) { return fs_exists_nothrow(fs_from_rt(p)); }
bool pathlib_is_file(void* p) { return fs_is_regular_nothrow(fs_from_rt(p)); }
bool pathlib_is_dir(void* p) { return fs_is_dir_nothrow(fs_from_rt(p)); }
bool pathlib_mkdir(void* p, int mode, int parents, int exist_ok) {
  (void)mode; // Permissions not modeled in this subset
  auto path = fs_from_rt(p);
  std::error_code ec;
  if (parents) {
    if (std::filesystem::create_directories(path, ec)) return true;
  } else {
    if (std::filesystem::create_directory(path, ec)) return true;
  }
  if (exist_ok && fs_is_dir_nothrow(path)) return true;
  return false;
}
bool pathlib_rmdir(void* p) {
  std::error_code ec; return std::filesystem::remove(fs_from_rt(p), ec);
}
bool pathlib_unlink(void* p) {
  std::error_code ec; return std::filesystem::remove(fs_from_rt(p), ec);
}
bool pathlib_rename(void* src, void* dst) {
  std::error_code ec; std::filesystem::rename(fs_from_rt(src), fs_from_rt(dst), ec); return !ec;
}

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

// C ABI wrappers for pathlib
extern "C" void* pycc_pathlib_cwd() { return ::pycc::rt::pathlib_cwd(); }
extern "C" void* pycc_pathlib_home() { return ::pycc::rt::pathlib_home(); }
extern "C" void* pycc_pathlib_join2(void* a, void* b) { return ::pycc::rt::pathlib_join2(a,b); }
extern "C" void* pycc_pathlib_parent(void* p) { return ::pycc::rt::pathlib_parent(p); }
extern "C" void* pycc_pathlib_basename(void* p) { return ::pycc::rt::pathlib_basename(p); }
extern "C" void* pycc_pathlib_suffix(void* p) { return ::pycc::rt::pathlib_suffix(p); }
extern "C" void* pycc_pathlib_stem(void* p) { return ::pycc::rt::pathlib_stem(p); }
extern "C" void* pycc_pathlib_with_name(void* p, void* n) { return ::pycc::rt::pathlib_with_name(p,n); }
extern "C" void* pycc_pathlib_with_suffix(void* p, void* s) { return ::pycc::rt::pathlib_with_suffix(p,s); }
extern "C" void* pycc_pathlib_as_posix(void* p) { return ::pycc::rt::pathlib_as_posix(p); }
extern "C" void* pycc_pathlib_as_uri(void* p) { return ::pycc::rt::pathlib_as_uri(p); }
extern "C" void* pycc_pathlib_resolve(void* p) { return ::pycc::rt::pathlib_resolve(p); }
extern "C" void* pycc_pathlib_absolute(void* p) { return ::pycc::rt::pathlib_absolute(p); }
extern "C" void* pycc_pathlib_parts(void* p) { return ::pycc::rt::pathlib_parts(p); }
extern "C" bool  pycc_pathlib_match(void* p, void* pat) { return ::pycc::rt::pathlib_match(p,pat); }
extern "C" bool  pycc_pathlib_exists(void* p) { return ::pycc::rt::pathlib_exists(p); }
extern "C" bool  pycc_pathlib_is_file(void* p) { return ::pycc::rt::pathlib_is_file(p); }
extern "C" bool  pycc_pathlib_is_dir(void* p) { return ::pycc::rt::pathlib_is_dir(p); }
extern "C" bool  pycc_pathlib_mkdir(void* p, int m, int parents, int exist_ok) { return ::pycc::rt::pathlib_mkdir(p,m,parents,exist_ok); }
extern "C" bool  pycc_pathlib_rmdir(void* p) { return ::pycc::rt::pathlib_rmdir(p); }
extern "C" bool  pycc_pathlib_unlink(void* p) { return ::pycc::rt::pathlib_unlink(p); }
extern "C" bool  pycc_pathlib_rename(void* a, void* b) { return ::pycc::rt::pathlib_rename(a,b); }

// ===== os.path module (subset wrappers) =====
namespace pycc::rt {

void* os_path_join2(void* a, void* b) { return pathlib_join2(a,b); }
void* os_path_dirname(void* p) { return pathlib_parent(p); }
void* os_path_basename(void* p) { return pathlib_basename(p); }
void* os_path_abspath(void* p) { return pathlib_absolute(p); }
bool  os_path_exists(void* p) { return pathlib_exists(p); }
bool  os_path_isfile(void* p) { return pathlib_is_file(p); }
bool  os_path_isdir(void* p) { return pathlib_is_dir(p); }
void* os_path_splitext(void* p) {
  if (!p) return list_new(2);
  // ext via pathlib_suffix; root by removing ext from end if present
  void* ext = pathlib_suffix(p);
  std::string path(string_data(p), string_len(p));
  std::string e(string_data(ext), string_len(ext));
  std::string root = path;
  if (!e.empty() && path.size() >= e.size()) {
    if (path.compare(path.size()-e.size(), e.size(), e) == 0) {
      root = path.substr(0, path.size() - e.size());
    }
  }
  void* out = list_new(2);
  list_push_slot(&out, string_new(root.data(), root.size()));
  list_push_slot(&out, ext);
  return out;
}

} // namespace pycc::rt

// C ABI for os.path
extern "C" void* pycc_os_path_join2(void* a, void* b) { return ::pycc::rt::os_path_join2(a,b); }
extern "C" void* pycc_os_path_dirname(void* p) { return ::pycc::rt::os_path_dirname(p); }
extern "C" void* pycc_os_path_basename(void* p) { return ::pycc::rt::os_path_basename(p); }
extern "C" void* pycc_os_path_splitext(void* p) { return ::pycc::rt::os_path_splitext(p); }
extern "C" void* pycc_os_path_abspath(void* p) { return ::pycc::rt::os_path_abspath(p); }
extern "C" bool  pycc_os_path_exists(void* p) { return ::pycc::rt::os_path_exists(p); }
extern "C" bool  pycc_os_path_isfile(void* p) { return ::pycc::rt::os_path_isfile(p); }
extern "C" bool  pycc_os_path_isdir(void* p) { return ::pycc::rt::os_path_isdir(p); }

// ===== colorsys module (subset) =====
namespace pycc::rt {

static inline double clamp01(double x) { if (x < 0.0) return 0.0; if (x > 1.0) return 1.0; return x; }

void* colorsys_rgb_to_hsv(double r, double g, double b) {
  r = clamp01(r); g = clamp01(g); b = clamp01(b);
  double maxc = std::max(r, std::max(g, b));
  double minc = std::min(r, std::min(g, b));
  double v = maxc;
  double s, h;
  if (minc == maxc) {
    s = 0.0; h = 0.0;
  } else {
    double diff = maxc - minc;
    s = (maxc <= 0.0) ? 0.0 : (diff / maxc);
    if (r == maxc) h = (g - b) / diff;
    else if (g == maxc) h = 2.0 + (b - r) / diff;
    else h = 4.0 + (r - g) / diff;
    h /= 6.0;
    if (h < 0.0) h += 1.0;
  }
  void* out = list_new(3);
  list_push_slot(&out, box_float(h));
  list_push_slot(&out, box_float(s));
  list_push_slot(&out, box_float(v));
  return out;
}

void* colorsys_hsv_to_rgb(double h, double s, double v) {
  h = clamp01(h); s = clamp01(s); v = clamp01(v);
  double r, g, b;
  if (s == 0.0) { r = g = b = v; }
  else {
    double hh = h * 6.0;
    int i = static_cast<int>(std::floor(hh)) % 6;
    double f = hh - std::floor(hh);
    double p = v * (1.0 - s);
    double q = v * (1.0 - s * f);
    double t = v * (1.0 - s * (1.0 - f));
    switch (i) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
    }
  }
  void* out = list_new(3);
  list_push_slot(&out, box_float(r));
  list_push_slot(&out, box_float(g));
  list_push_slot(&out, box_float(b));
  return out;
}

} // namespace pycc::rt

// C ABI for colorsys
extern "C" void* pycc_colorsys_rgb_to_hsv(double r, double g, double b) { return ::pycc::rt::colorsys_rgb_to_hsv(r,g,b); }
extern "C" void* pycc_colorsys_hsv_to_rgb(double h, double s, double v) { return ::pycc::rt::colorsys_hsv_to_rgb(h,s,v); }

// C ABI wrappers for _abc
extern "C" long long pycc_abc_get_cache_token() { return ::pycc::rt::abc_get_cache_token(); }
extern "C" int  pycc_abc_register(void* a, void* b) { return ::pycc::rt::abc_register(a,b) ? 1 : 0; }
extern "C" int  pycc_abc_is_registered(void* a, void* b) { return ::pycc::rt::abc_is_registered(a,b) ? 1 : 0; }
extern "C" void pycc_abc_invalidate_cache() { ::pycc::rt::abc_invalidate_cache(); }
extern "C" void pycc_abc_reset() { ::pycc::rt::abc_reset(); }

// C ABI wrappers for _aix_support
extern "C" void* pycc_aix_platform() { return ::pycc::rt::aix_platform(); }
extern "C" void* pycc_aix_default_libpath() { return ::pycc::rt::aix_default_libpath(); }
extern "C" void* pycc_aix_ldflags() { return ::pycc::rt::aix_ldflags(); }

// C ABI wrappers for _android_support
extern "C" void* pycc_android_platform() { return ::pycc::rt::android_platform(); }
extern "C" void* pycc_android_default_libdir() { return ::pycc::rt::android_default_libdir(); }
extern "C" void* pycc_android_ldflags() { return ::pycc::rt::android_ldflags(); }

// C ABI wrappers for _apple_support
extern "C" void* pycc_apple_platform() { return ::pycc::rt::apple_platform(); }
extern "C" void* pycc_apple_default_sdkroot() { return ::pycc::rt::apple_default_sdkroot(); }
extern "C" void* pycc_apple_ldflags() { return ::pycc::rt::apple_ldflags(); }

// C ABI wrappers for _ast
extern "C" void* pycc_ast_dump(void* o) { return ::pycc::rt::ast_dump(o); }
extern "C" void* pycc_ast_iter_fields(void* o) { return ::pycc::rt::ast_iter_fields(o); }
extern "C" void* pycc_ast_walk(void* o) { return ::pycc::rt::ast_walk(o); }
extern "C" void* pycc_ast_copy_location(void* a, void* b) { return ::pycc::rt::ast_copy_location(a,b); }
extern "C" void* pycc_ast_fix_missing_locations(void* n) { return ::pycc::rt::ast_fix_missing_locations(n); }
extern "C" void* pycc_ast_get_docstring(void* n) { return ::pycc::rt::ast_get_docstring(n); }

// C ABI wrappers for _asyncio
extern "C" void* pycc_asyncio_get_event_loop() { return ::pycc::rt::asyncio_get_event_loop(); }
extern "C" void* pycc_asyncio_future_new() { return ::pycc::rt::asyncio_future_new(); }
extern "C" void  pycc_asyncio_future_set_result(void* f, void* r) { ::pycc::rt::asyncio_future_set_result(f,r); }
extern "C" void* pycc_asyncio_future_result(void* f) { return ::pycc::rt::asyncio_future_result(f); }
extern "C" int   pycc_asyncio_future_done(void* f) { return ::pycc::rt::asyncio_future_done(f) ? 1 : 0; }
extern "C" void  pycc_asyncio_sleep(double s) { ::pycc::rt::asyncio_sleep(s); }

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

// _abc registry and cache token
static std::mutex g_abc_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::unordered_map<void*, std::unordered_set<void*>> g_abc_registry; // NOLINT
static std::atomic<long long> g_abc_token{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

int64_t abc_get_cache_token() { return g_abc_token.load(std::memory_order_acquire); }
bool abc_register(void* abc, void* subclass) {
  if (!abc || !subclass) return false;
  const std::lock_guard<std::mutex> lk(g_abc_mu);
  auto& reg = g_abc_registry[abc];
  auto [it, inserted] = reg.insert(subclass);
  if (inserted) { g_abc_token.fetch_add(1, std::memory_order_acq_rel); }
  return inserted;
}
bool abc_is_registered(void* abc, void* subclass) {
  if (!abc || !subclass) return false;
  const std::lock_guard<std::mutex> lk(g_abc_mu);
  auto it = g_abc_registry.find(abc);
  if (it == g_abc_registry.end()) return false;
  return it->second.find(subclass) != it->second.end();
}
void abc_invalidate_cache() { g_abc_token.fetch_add(1, std::memory_order_acq_rel); }
void abc_reset() {
  const std::lock_guard<std::mutex> lk(g_abc_mu);
  g_abc_registry.clear();
  g_abc_token.store(0, std::memory_order_release);
}

// _aix_support shims (portable)
void* aix_platform() { return string_from_cstr("aix"); }
void* aix_default_libpath() { return string_from_cstr(""); }
void* aix_ldflags() { return list_new(0); }

// _android_support shims (portable)
void* android_platform() { return string_from_cstr("android"); }
void* android_default_libdir() { return string_from_cstr(""); }
void* android_ldflags() { return list_new(0); }

// _apple_support shims (portable)
void* apple_platform() { return string_from_cstr("darwin"); }
void* apple_default_sdkroot() { return string_from_cstr(""); }
void* apple_ldflags() { return list_new(0); }

// _ast shims (portable)
void* ast_dump(void* /*obj*/) { return string_from_cstr(""); }
void* ast_iter_fields(void* /*obj*/) { return list_new(0); }
void* ast_walk(void* /*obj*/) { return list_new(0); }
void* ast_copy_location(void* new_node, void* /*old_node*/) { return new_node; }
void* ast_fix_missing_locations(void* node) { return node; }
void* ast_get_docstring(void* /*node*/) { return string_from_cstr(""); }

// _asyncio shims
void* asyncio_get_event_loop() { return object_new(0); }
void* asyncio_future_new() { return object_new(2); }
void asyncio_future_set_result(void* fut, void* result) {
  if (!fut) return;
  object_set(fut, 0, result);
  void* done = box_bool(true);
  object_set(fut, 1, done);
}
void* asyncio_future_result(void* fut) {
  if (!fut) return nullptr;
  return object_get(fut, 0);
}
bool asyncio_future_done(void* fut) {
  if (!fut) return false;
  void* d = object_get(fut, 1);
  if (!d) return false;
  return box_bool_value(d);
}
void asyncio_sleep(double seconds) { time_sleep(seconds); }
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
  //
  ch->cv_not_full.wait(lk, [&]{ return ch->closed || ch->q.size() < ch->cap; });
  if (ch->closed) return;
  ch->q.push_back(value);
  lk.unlock();
  ch->cv_not_empty.notify_one();
  //
}
void* chan_recv(RtChannelHandle* handle) {
  auto* ch = reinterpret_cast<Chan*>(handle); if (!ch) return nullptr;
  std::unique_lock<std::mutex> lk(ch->mu);
  //
  ch->cv_not_empty.wait(lk, [&]{ return ch->closed || !ch->q.empty(); });
  if (ch->q.empty()) return nullptr; // closed
  void* v = ch->q.front(); ch->q.pop_front();
  lk.unlock(); ch->cv_not_full.notify_one();
  //
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

TypeTag type_of_public(void* obj) { return type_of(obj); }

// removed: superseded by json_dump_str with UTF-8/ensure_ascii
// (indent_nl moved to JSON handlers)

// DumpOpts moved to runtime/detail/JsonTypes.h

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
      detail::json_dump_list(obj, out, opts, depth, &json_dumps_rec);
      return;
    }
    case TypeTag::Dict: {
      detail::json_dump_dict(obj, out, opts, depth, &json_dumps_rec);
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
  static inline int hexval(char c){ if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return 10+(c-'a'); if(c>='A'&&c<='F') return 10+(c-'A'); return -1; }
  bool read_u4(uint32_t& cp){
    if (i + 4 > n) return false;
    int h1 = hexval(s[i]), h2 = hexval(s[i+1]), h3 = hexval(s[i+2]), h4 = hexval(s[i+3]);
    if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) return false;
    cp = static_cast<uint32_t>((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
    i += 4;
    return true;
  }
  bool read_surrogate_pair(uint32_t hi, uint32_t& out_cp){
    if (!(i + 6 <= n && s[i] == '\\' && s[i+1] == 'u')) return false;
    i += 2;
    uint32_t low = 0;
    if (!read_u4(low)) return false;
    if (!(low >= 0xDC00 && low <= 0xDFFF)) return false;
    out_cp = 0x10000 + (((hi - 0xD800) << 10) | (low - 0xDC00));
    return true;
  }
  bool parse_unicode_after_u(std::string& out){
    uint32_t cp = 0;
    if (!read_u4(cp)) { rt_raise("ValueError", "json: invalid unicode escape"); return false; }
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      uint32_t full = 0;
      if (!read_surrogate_pair(cp, full)) { rt_raise("ValueError", "json: invalid unicode surrogate"); return false; }
      cp = full;
    }
    append_utf8(cp, out);
    return true;
  }
  bool parse_escape(std::string& out){
    if (i >= n) { rt_raise("ValueError", "json: invalid escape"); return false; }
    char e = s[i++];
    switch (e) {
      case '"': out.push_back('"'); break;
      case '\\': out.push_back('\\'); break;
      case '/': out.push_back('/'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      case 'u': return parse_unicode_after_u(out);
      default: out.push_back(e); break;
    }
    return true;
  }
  void* parseString(){
    skipws(); if (i>=n || s[i] != '"') { rt_raise("ValueError","json: expected string"); return nullptr; }
    ++i; std::string out;
    while (i < n) {
      char c = s[i++];
      if (c == '"') break;
      if (c == '\\') {
        if (!parse_escape(out)) return nullptr;
      } else {
        out.push_back(c);
      }
    }
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
  std::unordered_map<long long, void*> intCache;
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

// ---------------------------
// re module (simplified, std::regex-based)
// ---------------------------
#include <regex>
namespace pycc::rt {

static inline std::string rt_to_stdstr(void* s) {
  const char* d = string_data(s);
  const std::size_t n = string_len(s);
  return std::string(d ? d : "", n);
}

static std::string apply_flags_to_pattern(const std::string& pat, int flags) {
  // Flags bits: 0x02 = IGNORECASE (handled in regex flags), 0x08 = MULTILINE, 0x20 = DOTALL
  const bool multiline = (flags & 0x08) != 0;
  const bool dotall = (flags & 0x20) != 0;
  if (!multiline && !dotall) return pat;
  std::string out;
  out.reserve(pat.size() * 2);
  bool inClass = false; bool esc = false;
  for (size_t i = 0; i < pat.size(); ++i) {
    char c = pat[i];
    if (esc) { out.push_back(c); esc = false; continue; }
    if (c == '\\') { out.push_back(c); esc = true; continue; }
    if (c == '[') { inClass = true; out.push_back(c); continue; }
    if (c == ']' && inClass) { inClass = false; out.push_back(c); continue; }
    if (!inClass && dotall && c == '.') {
      out += "[\\s\\S]"; // match any including newline
      continue;
    }
    if (!inClass && multiline && (c == '^' || c == '$')) {
      if (c == '^') {
        out += "(?:^|\n)"; // approximate multiline start (consumes newline)
      } else {
        out += "(?:\n|$)"; // approximate multiline end (consumes newline)
      }
      continue;
    }
    out.push_back(c);
  }
  return out;
}

static std::regex make_regex(const std::string& pat, int flags) {
  std::regex::flag_type f = std::regex::ECMAScript;
  if (flags & 0x02 /* IGNORECASE */) { f |= std::regex::icase; }
  // Translate flags for MULTILINE (0x08) and DOTALL (0x20) by editing the pattern.
  std::string p2 = apply_flags_to_pattern(pat, flags);
  return std::regex(p2, f);
}

void* re_compile(void* pattern, int flags) {
  // Store pattern and flags in an object for demonstration (not used in codegen path)
  void* obj = object_new(2);
  object_set(obj, 0, pattern);
  object_set(obj, 1, box_int(flags));
  return obj;
}

static inline void* make_match_obj(int start, int end, const std::string& group0) {
  void* m = object_new(3);
  object_set(m, 0, box_int(start));
  object_set(m, 1, box_int(end));
  object_set(m, 2, string_new(group0.data(), group0.size()));
  return m;
}

void* re_search(void* pattern, void* text, int flags) {
  try {
    std::string pat = rt_to_stdstr(pattern);
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    std::smatch m;
    if (std::regex_search(s, m, re)) {
      int start = static_cast<int>(m.position());
      int end = start + static_cast<int>(m.length());
      return make_match_obj(start, end, m.str());
    }
    return nullptr;
  } catch (...) { return nullptr; }
}

void* re_match(void* pattern, void* text, int flags) {
  try {
    std::string pat = std::string("^") + rt_to_stdstr(pattern);
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    std::smatch m;
    if (std::regex_search(s, m, re)) {
      if (m.position() == 0) {
        int start = 0; int end = static_cast<int>(m.length());
        return make_match_obj(start, end, m.str());
      }
    }
    return nullptr;
  } catch (...) { return nullptr; }
}

void* re_fullmatch(void* pattern, void* text, int flags) {
  try {
    std::string pat = std::string("^") + rt_to_stdstr(pattern) + std::string("$");
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    std::smatch m;
    if (std::regex_match(s, m, re)) {
      return make_match_obj(0, static_cast<int>(s.size()), m.str());
    }
    return nullptr;
  } catch (...) { return nullptr; }
}

void* re_findall(void* pattern, void* text, int flags) {
  try {
    std::string pat = rt_to_stdstr(pattern);
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    void* out = list_new(0);
    for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it) {
      const auto& m = *it;
      void* ms = string_new(m.str().data(), m.str().size());
      list_push_slot(&out, ms);
    }
    return out;
  } catch (...) { return list_new(0); }
}

void* re_finditer(void* pattern, void* text, int flags) {
  try {
    std::string pat = rt_to_stdstr(pattern);
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    void* out = list_new(0);
    for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it) {
      const auto& m = *it;
      int start = static_cast<int>(m.position());
      int end = start + static_cast<int>(m.length());
      void* mo = make_match_obj(start, end, m.str());
      list_push_slot(&out, mo);
    }
    return out;
  } catch (...) { return list_new(0); }
}

void* re_split(void* pattern, void* text, int maxsplit, int flags) {
  try {
    std::string pat = rt_to_stdstr(pattern);
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    void* out = list_new(0);
    std::sregex_token_iterator end;
    int splits = 0;
    std::size_t last = 0;
    for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it) {
      std::size_t pos = static_cast<std::size_t>((*it).position());
      std::size_t len = static_cast<std::size_t>((*it).length());
      std::string chunk = s.substr(last, pos - last);
      list_push_slot(&out, string_new(chunk.data(), chunk.size()));
      last = pos + len;
      ++splits;
      if (maxsplit > 0 && splits >= maxsplit) break;
    }
    std::string tail = s.substr(last);
    list_push_slot(&out, string_new(tail.data(), tail.size()));
    return out;
  } catch (...) { return list_new(0); }
}

static std::string translate_repl_backrefs(const std::string& repl) {
  std::string out; out.reserve(repl.size());
  for (std::size_t i = 0; i < repl.size(); ++i) {
    char c = repl[i];
    if (c == '\\' && i + 1 < repl.size() && std::isdigit(static_cast<unsigned char>(repl[i+1]))) {
      out.push_back('$'); out.push_back(repl[i+1]); ++i; continue;
    }
    out.push_back(c);
  }
  return out;
}

void* re_sub(void* pattern, void* repl, void* text, int count, int flags) {
  try {
    std::string pat = rt_to_stdstr(pattern);
    std::string rep = translate_repl_backrefs(rt_to_stdstr(repl));
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    if (count <= 0) {
      std::string res = std::regex_replace(s, re, rep);
      return string_new(res.data(), res.size());
    }
    // Limited count: manual loop
    std::string out; out.reserve(s.size());
    std::string::const_iterator searchStart(s.cbegin());
    int replaced = 0;
    std::smatch m;
    while (replaced < count && std::regex_search(searchStart, s.cend(), m, re)) {
      out.append(searchStart, m.prefix().second);
      out.append(m.format(rep));
      searchStart = m.suffix().first;
      ++replaced;
    }
    out.append(searchStart, s.cend());
    return string_new(out.data(), out.size());
  } catch (...) { return nullptr; }
}

void* re_subn(void* pattern, void* repl, void* text, int count, int flags) {
  try {
    std::string pat = rt_to_stdstr(pattern);
    std::string rep = translate_repl_backrefs(rt_to_stdstr(repl));
    std::string s = rt_to_stdstr(text);
    std::regex re = make_regex(pat, flags);
    std::string out;
    out.reserve(s.size());
    std::string::const_iterator searchStart(s.cbegin());
    int replaced = 0;
    std::smatch m;
    while ((count <= 0 || replaced < count) && std::regex_search(searchStart, s.cend(), m, re)) {
      out.append(searchStart, m.prefix().second);
      out.append(m.format(rep));
      searchStart = m.suffix().first;
      ++replaced;
    }
    out.append(searchStart, s.cend());
    void* res = list_new(2);
    list_push_slot(&res, string_new(out.data(), out.size()));
    list_push_slot(&res, box_int(replaced));
    return res;
  } catch (...) { return list_new(2); }
}

void* re_escape(void* text) {
  std::string s = rt_to_stdstr(text);
  std::string out; out.reserve(s.size() * 2);
  auto is_alnum = [](unsigned char c){ return std::isalnum(c); };
  for (unsigned char c : s) {
    if (is_alnum(c)) out.push_back(static_cast<char>(c));
    else { out.push_back('\\'); out.push_back(static_cast<char>(c)); }
  }
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI exports for re
extern "C" void* pycc_re_compile(void* p, int f) { return ::pycc::rt::re_compile(p,f); }
extern "C" void* pycc_re_search(void* p, void* t, int f) { return ::pycc::rt::re_search(p,t,f); }
extern "C" void* pycc_re_match(void* p, void* t, int f) { return ::pycc::rt::re_match(p,t,f); }
extern "C" void* pycc_re_fullmatch(void* p, void* t, int f) { return ::pycc::rt::re_fullmatch(p,t,f); }
extern "C" void* pycc_re_findall(void* p, void* t, int f) { return ::pycc::rt::re_findall(p,t,f); }
extern "C" void* pycc_re_split(void* p, void* t, int maxs, int f) { return ::pycc::rt::re_split(p,t,maxs,f); }
extern "C" void* pycc_re_sub(void* p, void* r, void* t, int c, int f) { return ::pycc::rt::re_sub(p,r,t,c,f); }
extern "C" void* pycc_re_subn(void* p, void* r, void* t, int c, int f) { return ::pycc::rt::re_subn(p,r,t,c,f); }
extern "C" void* pycc_re_escape(void* s) { return ::pycc::rt::re_escape(s); }
extern "C" void* pycc_re_finditer(void* p, void* t, int f) { return ::pycc::rt::re_finditer(p,t,f); }

// ===== fnmatch module =====
namespace pycc::rt {

static inline std::string rt_str(void* s) {
  if (!s) return std::string();
  const char* d = string_data(s);
  const std::size_t n = string_len(s);
  return std::string(d ? d : "", n);
}

static bool is_windows_like() {
#if defined(_WIN32) || defined(_WIN64)
  return true;
#else
  return false;
#endif
}

static std::string regex_escape_lit(char c) {
  switch (c) {
    case '.': case '+': case '(': case ')': case '{': case '}': case '^': case '$': case '|': case '\\':
      return std::string("\\") + c;
    default: return std::string(1, c);
  }
}

static std::string fnmatch_to_regex(const std::string& pat) {
  std::string out; out.reserve(pat.size() * 2);
  for (size_t i = 0; i < pat.size(); ++i) {
    char c = pat[i];
    if (c == '*') { out += ".*"; continue; }
    if (c == '?') { out += "."; continue; }
    if (c == '[') {
      // Find closing ']' to decide if this is a class; if not found, treat '[' literally
      size_t j = i + 1;
      bool hasClose = false;
      for (size_t k = j; k < pat.size(); ++k) {
        if (pat[k] == ']') { hasClose = true; break; }
      }
      if (!hasClose) { out += "\\["; continue; }
      // Build character class with support for leading '!' or '^' negation and literal ']' as first char
      out.push_back('[');
      bool negate = false;
      if (j < pat.size() && (pat[j] == '!' || pat[j] == '^')) { negate = true; ++j; }
      if (negate) out.push_back('^');
      // If the next char is ']' it is taken literally inside the class
      if (j < pat.size() && pat[j] == ']') { out.push_back(']'); ++j; }
      // Emit the rest of class characters verbatim until closing ']'
      while (j < pat.size()) {
        char cc = pat[j];
        out.push_back(cc);
        if (cc == ']') { break; }
        ++j;
      }
      // Advance main index to closing ']' (or end if unmatched which shouldn't happen here)
      i = (j < pat.size() ? j : pat.size() - 1);
      continue;
    }
    out += regex_escape_lit(c);
  }
  return std::string("^") + out + std::string("$");
}

bool fnmatch_fnmatchcase(void* name, void* pattern) {
  try {
    std::string n = rt_str(name);
    std::string p = rt_str(pattern);
    std::regex re(fnmatch_to_regex(p));
    return std::regex_match(n, re);
  } catch (...) { return false; }
}

bool fnmatch_fnmatch(void* name, void* pattern) {
  try {
    std::string n = rt_str(name);
    std::string p = rt_str(pattern);
    if (is_windows_like()) {
      std::transform(n.begin(), n.end(), n.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
      std::transform(p.begin(), p.end(), p.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    }
    std::regex re(fnmatch_to_regex(p));
    return std::regex_match(n, re);
  } catch (...) { return false; }
}

void* fnmatch_filter(void* names_list, void* pattern) {
  std::string p = rt_str(pattern);
  std::regex re(fnmatch_to_regex(p));
  void* out = list_new(0);
  if (!names_list) return out;
  const std::size_t n = list_len(names_list);
  for (std::size_t i = 0; i < n; ++i) {
    void* s = list_get(names_list, i);
    std::string name = rt_str(s);
    if (std::regex_match(name, re)) list_push_slot(&out, s);
  }
  return out;
}

void* fnmatch_translate(void* pattern) {
  std::string p = rt_str(pattern);
  std::string rx = fnmatch_to_regex(p);
  return string_new(rx.data(), rx.size());
}

} // namespace pycc::rt

// C ABI for fnmatch
extern "C" int  pycc_fnmatch_fnmatch(void* n, void* p) { return ::pycc::rt::fnmatch_fnmatch(n,p) ? 1 : 0; }
extern "C" int  pycc_fnmatch_fnmatchcase(void* n, void* p) { return ::pycc::rt::fnmatch_fnmatchcase(n,p) ? 1 : 0; }
extern "C" void* pycc_fnmatch_filter(void* names, void* p) { return ::pycc::rt::fnmatch_filter(names,p); }
extern "C" void* pycc_fnmatch_translate(void* p) { return ::pycc::rt::fnmatch_translate(p); }

// ===== string module =====
namespace pycc::rt {

static inline void capitalize_word(std::string& w) {
  if (w.empty()) return;
  // lowercase rest then uppercase first char (ASCII-focused)
  for (size_t i = 0; i < w.size(); ++i) {
    unsigned char ch = static_cast<unsigned char>(w[i]);
    if (i == 0) w[i] = static_cast<char>(std::toupper(ch));
    else w[i] = static_cast<char>(std::tolower(ch));
  }
}

static std::vector<std::string> split_whitespace(const std::string& s) {
  std::vector<std::string> out;
  size_t i = 0, n = s.size();
  while (i < n) {
    while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i >= n) break;
    size_t j = i;
    while (j < n && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
    out.emplace_back(s.substr(i, j - i));
    i = j;
  }
  return out;
}

static std::vector<std::string> split_on_sep(const std::string& s, const std::string& sep) {
  std::vector<std::string> out;
  if (sep.empty()) { out.push_back(s); return out; }
  size_t pos = 0;
  while (true) {
    size_t found = s.find(sep, pos);
    if (found == std::string::npos) { out.emplace_back(s.substr(pos)); break; }
    out.emplace_back(s.substr(pos, found - pos));
    pos = found + sep.size();
  }
  return out;
}

void* string_capwords(void* s, void* sep_or_null) {
  std::string in;
  if (s) { const char* d = string_data(s); in.assign(d ? d : "", string_len(s)); }
  std::vector<std::string> words;
  std::string sep;
  if (sep_or_null) {
    const char* ds = string_data(sep_or_null);
    sep.assign(ds ? ds : "", string_len(sep_or_null));
    words = split_on_sep(in, sep);
    for (auto& w : words) capitalize_word(w);
    // Preserve separators by joining with same sep
    std::string out;
    if (!words.empty()) {
      out = words[0];
      for (size_t i = 1; i < words.size(); ++i) { out += sep; out += words[i]; }
    }
    return string_new(out.data(), out.size());
  }
  // Default: split on whitespace; join with single spaces
  words = split_whitespace(in);
  for (auto& w : words) capitalize_word(w);
  std::string out;
  for (size_t i = 0; i < words.size(); ++i) { if (i) out.push_back(' '); out += words[i]; }
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for string
extern "C" void* pycc_string_capwords(void* s, void* sep) { return ::pycc::rt::string_capwords(s, sep); }

// ===== glob module =====
namespace pycc::rt {

static std::string glob_norm(std::filesystem::path p) {
  return p.generic_string();
}

static std::string glob_escape_regex(const std::string& s) {
  std::string out; out.reserve(s.size() * 2);
  for (char c : s) {
    switch (c) {
      case '.': case '+': case '(': case ')': case '{': case '}': case '^': case '$': case '|': case '\\':
        out.push_back('\\'); out.push_back(c); break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

static std::string glob_pattern_to_regex(const std::string& pat) {
  std::string out; out.reserve(pat.size() * 2);
  for (size_t i = 0; i < pat.size(); ++i) {
    char c = pat[i];
    if (c == '*') {
      if (i + 1 < pat.size() && pat[i+1] == '*') { out += ".*"; ++i; }
      else out += "[^/]*";
      continue;
    }
    if (c == '?') { out += "[^/]"; continue; }
    if (c == '[') {
      // simple character class passthrough until ']'
      out.push_back('[');
      ++i;
      while (i < pat.size() && pat[i] != ']') { out.push_back(pat[i]); ++i; }
      if (i < pat.size()) out.push_back(']');
      continue;
    }
    out += glob_escape_regex(std::string(1, c));
  }
  return std::string("^") + out + std::string("$");
}

static void glob_collect_from(const std::filesystem::path& base, const std::regex& rx, std::vector<std::string>& out, bool recursive) {
  try {
    if (recursive) {
      for (auto it = std::filesystem::recursive_directory_iterator(base); it != std::filesystem::recursive_directory_iterator(); ++it) {
        std::string rel = glob_norm(std::filesystem::relative(it->path(), std::filesystem::current_path()));
        if (std::regex_match(rel, rx)) out.push_back(rel);
      }
    } else {
      for (auto& de : std::filesystem::directory_iterator(base)) {
        std::string rel = glob_norm(std::filesystem::relative(de.path(), std::filesystem::current_path()));
        if (std::regex_match(rel, rx)) out.push_back(rel);
      }
    }
  } catch (...) {
    // ignore errors
  }
}

void* glob_glob(void* pattern) {
  if (!pattern) return list_new(0);
  std::string pat = glob_norm(std::filesystem::path(rt_to_stdstr(pattern)));
  bool recursive = pat.find("**") != std::string::npos;
  std::regex rx(glob_pattern_to_regex(pat));
  std::vector<std::string> matches;
  // Determine a base directory: if pat contains '/', use prefix before first wildcard
  std::filesystem::path base = std::filesystem::current_path();
  auto pos = pat.rfind('/');
  if (pos != std::string::npos) {
    std::string dir = pat.substr(0, pos);
    try { base = std::filesystem::path(dir); if (!base.is_absolute()) base = std::filesystem::current_path() / base; } catch (...) {}
  }
  glob_collect_from(base, rx, matches, recursive);
  std::sort(matches.begin(), matches.end());
  void* lst = list_new(matches.size());
  for (const auto& m : matches) list_push_slot(&lst, string_new(m.data(), m.size()));
  return lst;
}

void* glob_iglob(void* pattern) { return glob_glob(pattern); }

void* glob_escape(void* pattern) {
  if (!pattern) return string_from_cstr("");
  std::string s = rt_to_stdstr(pattern);
  std::string out; out.reserve(s.size() * 2);
  for (char c : s) {
    if (c == '*' || c == '?' || c == '[' || c == ']') { out.push_back('\\'); out.push_back(c); }
    else out.push_back(c);
  }
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for glob
extern "C" void* pycc_glob_glob(void* p) { return ::pycc::rt::glob_glob(p); }
extern "C" void* pycc_glob_iglob(void* p) { return ::pycc::rt::glob_iglob(p); }
extern "C" void* pycc_glob_escape(void* p) { return ::pycc::rt::glob_escape(p); }

// ===== uuid module =====
namespace pycc::rt {

static inline unsigned char rnd_byte() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(0, 255);
  return static_cast<unsigned char>(dist(rng));
}

void* uuid_uuid4() {
  unsigned char bytes[16];
  for (auto & b : bytes) b = rnd_byte();
  // Set version (4)
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
  // Set variant (10xx)
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
  static const char* hex = "0123456789abcdef";
  char out[36];
  auto w2 = [&](int idx, unsigned char v){ out[idx] = hex[(v>>4)&0xF]; out[idx+1] = hex[v&0xF]; };
  int pos = 0;
  w2(pos, bytes[0]); pos+=2; w2(pos, bytes[1]); pos+=2; w2(pos, bytes[2]); pos+=2; w2(pos, bytes[3]); pos+=2; out[pos++]='-';
  w2(pos, bytes[4]); pos+=2; w2(pos, bytes[5]); pos+=2; out[pos++]='-';
  w2(pos, bytes[6]); pos+=2; w2(pos, bytes[7]); pos+=2; out[pos++]='-';
  w2(pos, bytes[8]); pos+=2; w2(pos, bytes[9]); pos+=2; out[pos++]='-';
  w2(pos, bytes[10]); pos+=2; w2(pos, bytes[11]); pos+=2; w2(pos, bytes[12]); pos+=2; w2(pos, bytes[13]); pos+=2; w2(pos, bytes[14]); pos+=2; w2(pos, bytes[15]); pos+=2;
  return string_new(out, sizeof(out));
}

} // namespace pycc::rt

// C ABI for uuid
extern "C" void* pycc_uuid_uuid4() { return ::pycc::rt::uuid_uuid4(); }

// ===== base64 module =====
namespace pycc::rt {

static std::vector<unsigned char> as_bytes(void* obj) {
  std::vector<unsigned char> out;
  if (!obj) return out;
  // Determine tag
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  TypeTag t = static_cast<TypeTag>(h->tag);
  if (t == TypeTag::Bytes) {
    const unsigned char* d = bytes_data(obj);
    out.assign(d, d + bytes_len(obj));
  } else if (t == TypeTag::String) {
    const char* d = string_data(obj);
    out.assign(reinterpret_cast<const unsigned char*>(d), reinterpret_cast<const unsigned char*>(d) + string_len(obj));
  }
  return out;
}

void* base64_b64encode(void* data) {
  static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  auto in = as_bytes(data);
  std::string out;
  out.reserve(((in.size()+2)/3)*4);
  size_t i = 0;
  while (i + 3 <= in.size()) {
    unsigned int n = (in[i] << 16) | (in[i+1] << 8) | in[i+2];
    out.push_back(tbl[(n >> 18) & 0x3F]);
    out.push_back(tbl[(n >> 12) & 0x3F]);
    out.push_back(tbl[(n >> 6) & 0x3F]);
    out.push_back(tbl[n & 0x3F]);
    i += 3;
  }
  if (i + 1 == in.size()) {
    unsigned int n = (in[i] << 16);
    out.push_back(tbl[(n >> 18) & 0x3F]);
    out.push_back(tbl[(n >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (i + 2 == in.size()) {
    unsigned int n = (in[i] << 16) | (in[i+1] << 8);
    out.push_back(tbl[(n >> 18) & 0x3F]);
    out.push_back(tbl[(n >> 12) & 0x3F]);
    out.push_back(tbl[(n >> 6) & 0x3F]);
    out.push_back('=');
  }
  return bytes_new(out.data(), out.size());
}

void* base64_b64decode(void* data) {
  auto in = as_bytes(data);
  std::string out;
  detail::base64_decode_bytes(in.data(), in.size(), out);
  return bytes_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for base64
extern "C" void* pycc_base64_b64encode(void* d) { return ::pycc::rt::base64_b64encode(d); }
extern "C" void* pycc_base64_b64decode(void* d) { return ::pycc::rt::base64_b64decode(d); }

// ===== random module =====
namespace pycc::rt {

static thread_local std::mt19937_64 g_rng{5489u}; // default MT seed

double random_random() {
  static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
  return dist(g_rng);
}

int32_t random_randint(int32_t a, int32_t b) {
  if (b < a) std::swap(a,b);
  std::uniform_int_distribution<int32_t> dist(a, b);
  return dist(g_rng);
}

void random_seed(uint64_t seed) { g_rng.seed(seed); }

} // namespace pycc::rt

// C ABI for random
extern "C" double pycc_random_random() { return ::pycc::rt::random_random(); }
extern "C" int32_t pycc_random_randint(int32_t a, int32_t b) { return ::pycc::rt::random_randint(a,b); }
extern "C" void pycc_random_seed(uint64_t s) { ::pycc::rt::random_seed(s); }

// ===== stat module =====
namespace pycc::rt {

int32_t stat_ifmt(int32_t mode) {
#ifdef S_IFMT
  return static_cast<int32_t>(mode & S_IFMT);
#else
  return static_cast<int32_t>(mode & 0170000);
#endif
}

bool stat_isdir(int32_t mode) {
#ifdef S_IFDIR
  return (mode & S_IFMT) == S_IFDIR;
#else
  return (mode & 0170000) == 0040000;
#endif
}

bool stat_isreg(int32_t mode) {
#ifdef S_IFREG
  return (mode & S_IFMT) == S_IFREG;
#else
  return (mode & 0170000) == 0100000;
#endif
}

} // namespace pycc::rt

// C ABI for stat
extern "C" int32_t pycc_stat_ifmt(int32_t m) { return ::pycc::rt::stat_ifmt(m); }
extern "C" int     pycc_stat_isdir(int32_t m) { return ::pycc::rt::stat_isdir(m) ? 1 : 0; }
extern "C" int     pycc_stat_isreg(int32_t m) { return ::pycc::rt::stat_isreg(m) ? 1 : 0; }

// ===== secrets module =====
namespace pycc::rt {

static thread_local std::mt19937_64 sec_rng{std::random_device{}()};

void* secrets_token_bytes(int32_t n) {
  if (n <= 0) return bytes_new(nullptr, 0);
  std::string out; out.resize(static_cast<size_t>(n));
  std::uniform_int_distribution<int> dist(0, 255);
  for (int32_t i = 0; i < n; ++i) out[static_cast<size_t>(i)] = static_cast<char>(dist(sec_rng));
  return bytes_new(out.data(), out.size());
}

void* secrets_token_hex(int32_t n) {
  static const char* hex = "0123456789abcdef";
  if (n <= 0) return string_from_cstr("");
  std::string out; out.resize(static_cast<size_t>(n) * 2);
  std::uniform_int_distribution<int> dist(0, 255);
  size_t pos = 0;
  for (int32_t i = 0; i < n; ++i) {
    unsigned char v = static_cast<unsigned char>(dist(sec_rng));
    out[pos++] = hex[(v >> 4) & 0xF];
    out[pos++] = hex[v & 0xF];
  }
  return string_new(out.data(), out.size());
}

void* secrets_token_urlsafe(int32_t n) {
  if (n <= 0) return string_from_cstr("");
  // Generate n random bytes, base64 encode, make urlsafe and strip '=' padding
  std::string raw; raw.resize(static_cast<size_t>(n));
  std::uniform_int_distribution<int> dist(0, 255);
  for (int32_t i = 0; i < n; ++i) raw[static_cast<size_t>(i)] = static_cast<char>(dist(sec_rng));
  void* b = bytes_new(raw.data(), raw.size());
  void* enc = base64_b64encode(b);
  std::string out(reinterpret_cast<const char*>(bytes_data(enc)), bytes_len(enc));
  for (char& c : out) { if (c == '+') c = '-'; else if (c == '/') c = '_'; }
  while (!out.empty() && out.back() == '=') out.pop_back();
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for secrets
extern "C" void* pycc_secrets_token_bytes(int n) { return ::pycc::rt::secrets_token_bytes(n); }
extern "C" void* pycc_secrets_token_hex(int n) { return ::pycc::rt::secrets_token_hex(n); }
extern "C" void* pycc_secrets_token_urlsafe(int n) { return ::pycc::rt::secrets_token_urlsafe(n); }

// ===== shutil module =====
namespace pycc::rt {

bool shutil_copyfile(void* src_path, void* dst_path) {
  if (!src_path || !dst_path) return false;
  const char* sp = string_data(src_path);
  const char* dp = string_data(dst_path);
  if (!sp || !dp) return false;
  void* content = io_read_file(sp);
  if (!content) return false;
  return io_write_file(dp, content);
}

bool shutil_copy(void* src_path, void* dst_path) { return shutil_copyfile(src_path, dst_path); }

} // namespace pycc::rt

// C ABI for shutil
extern "C" int pycc_shutil_copyfile(void* a, void* b) { return ::pycc::rt::shutil_copyfile(a,b) ? 1 : 0; }
extern "C" int pycc_shutil_copy(void* a, void* b) { return ::pycc::rt::shutil_copy(a,b) ? 1 : 0; }

// ===== platform module =====
namespace pycc::rt {

static std::string plat_uname_field(int which) {
#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
  struct utsname u{};
  if (uname(&u) == 0) {
    switch (which) {
      case 0: return std::string(u.sysname);
      case 1: return std::string(u.machine);
      case 2: return std::string(u.release);
      case 3: return std::string(u.version);
    }
  }
#endif
  return std::string("unknown");
}

void* platform_system() { auto s = plat_uname_field(0); return string_new(s.data(), s.size()); }
void* platform_machine() { auto s = plat_uname_field(1); return string_new(s.data(), s.size()); }
void* platform_release() { auto s = plat_uname_field(2); return string_new(s.data(), s.size()); }
void* platform_version() { auto s = plat_uname_field(3); return string_new(s.data(), s.size()); }

} // namespace pycc::rt

// C ABI for platform
extern "C" void* pycc_platform_system() { return ::pycc::rt::platform_system(); }
extern "C" void* pycc_platform_machine() { return ::pycc::rt::platform_machine(); }
extern "C" void* pycc_platform_release() { return ::pycc::rt::platform_release(); }
extern "C" void* pycc_platform_version() { return ::pycc::rt::platform_version(); }

// ===== errno module =====
namespace pycc::rt {

// removed unused errno_val_or (avoid -Werror unused)

int32_t errno_EPERM() {
#ifdef EPERM
  return EPERM;
#else
  return 1;
#endif
}
int32_t errno_ENOENT() {
#ifdef ENOENT
  return ENOENT;
#else
  return 2;
#endif
}
int32_t errno_EEXIST() {
#ifdef EEXIST
  return EEXIST;
#else
  return 17;
#endif
}
int32_t errno_EISDIR() {
#ifdef EISDIR
  return EISDIR;
#else
  return 21;
#endif
}
int32_t errno_ENOTDIR() {
#ifdef ENOTDIR
  return ENOTDIR;
#else
  return 20;
#endif
}
int32_t errno_EACCES() {
#ifdef EACCES
  return EACCES;
#else
  return 13;
#endif
}

} // namespace pycc::rt

// C ABI for errno
extern "C" int32_t pycc_errno_EPERM() { return ::pycc::rt::errno_EPERM(); }
extern "C" int32_t pycc_errno_ENOENT() { return ::pycc::rt::errno_ENOENT(); }
extern "C" int32_t pycc_errno_EEXIST() { return ::pycc::rt::errno_EEXIST(); }
extern "C" int32_t pycc_errno_EISDIR() { return ::pycc::rt::errno_EISDIR(); }
extern "C" int32_t pycc_errno_ENOTDIR() { return ::pycc::rt::errno_ENOTDIR(); }
extern "C" int32_t pycc_errno_EACCES() { return ::pycc::rt::errno_EACCES(); }

// ===== heapq module =====
namespace pycc::rt {

static inline double num_value_for_heap(void* v) {
  if (!v) return 0.0;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(v) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int: return static_cast<double>(box_int_value(v));
    case TypeTag::Float: return box_float_value(v);
    case TypeTag::Bool: return box_bool_value(v) ? 1.0 : 0.0;
    default: return reinterpret_cast<uintptr_t>(v) * 1.0; // fallback
  }
}

static void sift_up(void* lst, std::size_t idx) {
  while (idx > 0) {
    std::size_t p = (idx - 1) / 2;
    void* vi = list_get(lst, idx);
    void* vp = list_get(lst, p);
    if (num_value_for_heap(vi) < num_value_for_heap(vp)) { list_set(lst, idx, vp); list_set(lst, p, vi); idx = p; }
    else break;
  }
}

static void sift_down(void* lst, std::size_t idx) {
  std::size_t n = list_len(lst);
  while (true) {
    std::size_t l = 2 * idx + 1, r = l + 1, m = idx;
    void* v = list_get(lst, idx);
    if (l < n && num_value_for_heap(list_get(lst, l)) < num_value_for_heap(v)) m = l;
    if (r < n && num_value_for_heap(list_get(lst, r)) < num_value_for_heap(list_get(lst, m))) m = r;
    if (m == idx) break;
    void* vm = list_get(lst, m);
    list_set(lst, m, v);
    list_set(lst, idx, vm);
    idx = m;
  }
}

void heapq_heappush(void* lst, void* value) {
  if (!lst) return;
  list_push_slot(&lst, value);
  sift_up(lst, list_len(lst) - 1);
}

void* heapq_heappop(void* lst) {
  if (!lst) return nullptr;
  // directly manipulate list metadata: [0]=len, [1]=cap, [2..]=items
  auto* meta = reinterpret_cast<std::size_t*>(lst);
  std::size_t n = meta[0];
  if (n == 0) return nullptr;
  auto** items = reinterpret_cast<void**>(meta + 2);
  void* top = items[0];
  if (n == 1) { gc_pre_barrier(&items[0]); items[0] = nullptr; gc_write_barrier(&items[0], nullptr); meta[0] = 0; return top; }
  void* last = items[n - 1];
  gc_pre_barrier(&items[n - 1]); items[n - 1] = nullptr; gc_write_barrier(&items[n - 1], nullptr);
  meta[0] = n - 1;
  gc_pre_barrier(&items[0]); items[0] = last; gc_write_barrier(&items[0], last);
  sift_down(lst, 0);
  return top;
}

} // namespace pycc::rt

// C ABI for heapq
extern "C" void pycc_heapq_heappush(void* a, void* v) { ::pycc::rt::heapq_heappush(a,v); }
extern "C" void* pycc_heapq_heappop(void* a) { return ::pycc::rt::heapq_heappop(a); }

// ===== bisect module =====
namespace pycc::rt {

static inline double to_num_for_bisect(void* v) {
  if (!v) return 0.0;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(v) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int: return static_cast<double>(box_int_value(v));
    case TypeTag::Float: return box_float_value(v);
    case TypeTag::Bool: return box_bool_value(v) ? 1.0 : 0.0;
    default: return 0.0;
  }
}

int32_t bisect_left(void* lst, void* xv) {
  std::size_t n = lst ? list_len(lst) : 0;
  double x = to_num_for_bisect(xv);
  std::size_t lo = 0, hi = n;
  while (lo < hi) {
    std::size_t mid = (lo + hi) >> 1;
    double v = to_num_for_bisect(list_get(lst, mid));
    if (v < x) lo = mid + 1; else hi = mid;
  }
  return static_cast<int32_t>(lo);
}

int32_t bisect_right(void* lst, void* xv) {
  std::size_t n = lst ? list_len(lst) : 0;
  double x = to_num_for_bisect(xv);
  std::size_t lo = 0, hi = n;
  while (lo < hi) {
    std::size_t mid = (lo + hi) >> 1;
    double v = to_num_for_bisect(list_get(lst, mid));
    if (x < v) hi = mid; else lo = mid + 1;
  }
  return static_cast<int32_t>(lo);
}

} // namespace pycc::rt

// C ABI for bisect
extern "C" int32_t pycc_bisect_left(void* a, void* x) { return ::pycc::rt::bisect_left(a,x); }
extern "C" int32_t pycc_bisect_right(void* a, void* x) { return ::pycc::rt::bisect_right(a,x); }

// ===== tempfile module =====
namespace pycc::rt {

static std::string choose_tmp_dir() {
  const char* envs[] = {"TMPDIR", "TEMP", "TMP"};
  for (const char* e : envs) { const char* v = std::getenv(e); if (v && *v) return std::string(v); }
#if defined(_WIN32)
  return std::string(".");
#else
  return std::string("/tmp");
#endif
}

void* tempfile_gettempdir() {
  std::string d = choose_tmp_dir();
  return string_new(d.data(), d.size());
}

static std::string random_suffix(size_t n=8) {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  static const char* al = "abcdefghijklmnopqrstuvwxyz0123456789";
  std::uniform_int_distribution<int> dist(0, 35);
  std::string s; s.resize(n);
  for (size_t i=0;i<n;++i) s[i] = al[dist(rng)];
  return s;
}

void* tempfile_mkdtemp() {
  std::string base = choose_tmp_dir();
  for (int i=0;i<1000;++i) {
    std::string p = base + "/pycc_" + random_suffix();
    std::error_code ec; if (std::filesystem::create_directories(p, ec)) return string_new(p.data(), p.size());
  }
  return string_from_cstr("");
}

void* tempfile_mkstemp() {
  std::string base = choose_tmp_dir();
  for (int i=0;i<1000;++i) {
    std::string p = base + "/pycc_" + random_suffix() + ".tmp";
    // Try to write empty file
    FILE* f = std::fopen(p.c_str(), "wb");
    if (f) {
      std::fclose(f);
      void* lst = list_new(2);
      list_push_slot(&lst, box_int(0));
      list_push_slot(&lst, string_new(p.data(), p.size()));
      return lst;
    }
  }
  return list_new(0);
}

} // namespace pycc::rt

// C ABI for tempfile
extern "C" void* pycc_tempfile_gettempdir() { return ::pycc::rt::tempfile_gettempdir(); }
extern "C" void* pycc_tempfile_mkdtemp() { return ::pycc::rt::tempfile_mkdtemp(); }
extern "C" void* pycc_tempfile_mkstemp() { return ::pycc::rt::tempfile_mkstemp(); }

// ===== statistics module =====
namespace pycc::rt {

static inline bool to_num_for_stats(void* v, double& out) {
  if (!v) { out = 0.0; return false; }
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(v) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int: out = static_cast<double>(box_int_value(v)); return true;
    case TypeTag::Float: out = box_float_value(v); return true;
    case TypeTag::Bool: out = box_bool_value(v) ? 1.0 : 0.0; return true;
    default: out = 0.0; return false;
  }
}

double statistics_mean(void* lst) {
  if (!lst) return 0.0;
  std::size_t n = list_len(lst); if (n == 0) return 0.0;
  long double sum = 0.0L; std::size_t cnt = 0;
  for (std::size_t i=0;i<n;++i) { double v; if (to_num_for_stats(list_get(lst,i), v)) { sum += v; ++cnt; } }
  if (cnt == 0) return 0.0;
  return static_cast<double>(sum / static_cast<long double>(cnt));
}

double statistics_median(void* lst) {
  if (!lst) return 0.0;
  std::size_t n = list_len(lst); if (n == 0) return 0.0;
  std::vector<double> xs; xs.reserve(n);
  for (std::size_t i=0;i<n;++i) { double v; if (to_num_for_stats(list_get(lst,i), v)) xs.push_back(v); }
  if (xs.empty()) return 0.0;
  std::sort(xs.begin(), xs.end());
  std::size_t m = xs.size();
  if ((m & 1U) == 1U) return xs[m/2];
  double a = xs[m/2 - 1], b = xs[m/2];
  return (a + b) / 2.0;
}

double statistics_pvariance(void* lst) {
  if (!lst) return 0.0;
  std::size_t n = list_len(lst); if (n == 0) return 0.0;
  long double sum = 0.0L; std::size_t cnt = 0;
  for (std::size_t i=0;i<n;++i) { double v; if (to_num_for_stats(list_get(lst,i), v)) { sum += v; ++cnt; } }
  if (cnt == 0) return 0.0;
  long double mean = sum / static_cast<long double>(cnt);
  long double ss = 0.0L;
  for (std::size_t i=0;i<n;++i) { double v; if (to_num_for_stats(list_get(lst,i), v)) { long double d = static_cast<long double>(v) - mean; ss += d*d; } }
  long double var = ss / static_cast<long double>(cnt);
  return static_cast<double>(var);
}

double statistics_stdev(void* lst) {
  if (!lst) return 0.0;
  std::size_t n = list_len(lst); if (n < 2) return 0.0;
  long double sum = 0.0L; std::size_t cnt = 0;
  for (std::size_t i=0;i<n;++i) { double v; if (to_num_for_stats(list_get(lst,i), v)) { sum += v; ++cnt; } }
  if (cnt < 2) return 0.0;
  long double mean = sum / static_cast<long double>(cnt);
  long double ss = 0.0L;
  for (std::size_t i=0;i<n;++i) { double v; if (to_num_for_stats(list_get(lst,i), v)) { long double d = static_cast<long double>(v) - mean; ss += d*d; } }
  long double var = ss / static_cast<long double>(cnt - 1);
  double sd = std::sqrt(static_cast<double>(var));
  return sd;
}

} // namespace pycc::rt

// C ABI for statistics
extern "C" double pycc_statistics_mean(void* a) { return ::pycc::rt::statistics_mean(a); }
extern "C" double pycc_statistics_median(void* a) { return ::pycc::rt::statistics_median(a); }
extern "C" double pycc_statistics_pvariance(void* a) { return ::pycc::rt::statistics_pvariance(a); }
extern "C" double pycc_statistics_stdev(void* a) { return ::pycc::rt::statistics_stdev(a); }

// ===== textwrap module =====
namespace pycc::rt {

static std::vector<std::string> split_words_norm(const std::string& s) {
  std::vector<std::string> words; words.reserve(16);
  size_t i=0,n=s.size();
  while (i<n) {
    while (i<n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i>=n) break; size_t j=i; while (j<n && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
    words.emplace_back(s.substr(i, j-i)); i=j;
  }
  return words;
}

void* textwrap_fill(void* str, int32_t width) {
  if (!str || width <= 0) return string_from_cstr("");
  std::string s(string_data(str), string_len(str));
  auto words = split_words_norm(s);
  std::string out; out.reserve(s.size()+words.size());
  size_t col = 0;
  for (size_t i=0;i<words.size();++i) {
    const std::string& w = words[i];
    if (col == 0) {
      out += w; col = w.size();
    } else if (col + 1 + w.size() <= static_cast<size_t>(width)) {
      out.push_back(' '); out += w; col += 1 + w.size();
    } else {
      out.push_back('\n'); out += w; col = w.size();
    }
  }
  return string_new(out.data(), out.size());
}

void* textwrap_shorten(void* str, int32_t width) {
  if (!str || width <= 0) return string_from_cstr("");
  std::string s(string_data(str), string_len(str));
  auto words = split_words_norm(s);
  std::string out;
  const std::string ellipsis = "...";
  // if fits, return normalized string
  size_t total = 0; if (!words.empty()) { total = words[0].size(); for (size_t i=1;i<words.size();++i) total += 1 + words[i].size(); }
  if (total <= static_cast<size_t>(width)) {
    if (!words.empty()) { out = words[0]; for (size_t i=1;i<words.size();++i){ out.push_back(' '); out += words[i]; } }
    return string_new(out.data(), out.size());
  }
  // otherwise, fill up to width-3 and add '...'
  int avail = width - static_cast<int>(ellipsis.size()); if (avail < 0) avail = 0;
  size_t used = 0; bool first = true;
  for (const auto& w : words) {
    size_t need = (first ? w.size() : 1 + w.size());
    if (used + need > static_cast<size_t>(avail)) break;
    if (first) { out += w; used += w.size(); first = false; }
    else { out.push_back(' '); out += w; used += 1 + w.size(); }
  }
  out += ellipsis;
  return string_new(out.data(), out.size());
}

void* textwrap_wrap(void* str, int32_t width) {
  if (!str || width <= 0) return list_new(0);
  std::string s(string_data(str), string_len(str));
  auto words = split_words_norm(s);
  void* out = list_new(8);
  std::string line;
  size_t col = 0;
  auto flush_line = [&]() {
    if (!line.empty()) {
      void* ls = string_new(line.data(), line.size());
      list_push_slot(&out, ls);
      line.clear();
      col = 0;
    }
  };
  for (size_t i=0;i<words.size();++i) {
    const std::string& w = words[i];
    if (col == 0) {
      line = w; col = w.size();
    } else if (col + 1 + w.size() <= static_cast<size_t>(width)) {
      line.push_back(' '); line += w; col += 1 + w.size();
    } else {
      flush_line();
      line = w; col = w.size();
    }
  }
  flush_line();
  return out;
}

void* textwrap_dedent(void* str) {
  if (!str) return string_from_cstr("");
  std::string s(string_data(str), string_len(str));
  // Collect lines with their ending preserved
  std::vector<std::pair<std::string,bool>> lines; // {content_without_nl, had_nl}
  lines.reserve(16);
  size_t i=0, n=s.size();
  while (i<n) {
    size_t j = i; while (j<n && s[j] != '\n') ++j;
    std::string line = s.substr(i, j-i);
    bool had_nl = (j<n && s[j]=='\n');
    lines.emplace_back(std::move(line), had_nl);
    i = j + (had_nl?1:0);
  }
  // Compute minimal indentation across non-blank lines (spaces and tabs)
  size_t minIndent = SIZE_MAX;
  for (const auto& p : lines) {
    const std::string& ln = p.first;
    size_t k=0; while (k<ln.size() && (ln[k]==' ' || ln[k]=='\t')) ++k;
    if (k==ln.size()) continue; // blank or all whitespace; ignore
    if (k < minIndent) minIndent = k;
  }
  if (minIndent == SIZE_MAX) minIndent = 0; // no non-blank lines
  // Build output removing minIndent from each non-blank line
  std::string out;
  for (size_t idx=0; idx<lines.size(); ++idx) {
    const auto& p = lines[idx];
    const std::string& ln = p.first;
    size_t k=0; while (k<ln.size() && (ln[k]==' ' || ln[k]=='\t')) ++k;
    if (k==ln.size()) {
      // blank line
      // Keep as-is (no content) and preserve newline if present
    } else {
      size_t drop = std::min(minIndent, k);
      out.append(ln.substr(drop));
    }
    if (p.second) out.push_back('\n');
    else if (idx+1 < lines.size()) out.push_back('\n'); // preserve inter-line breaks
  }
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for textwrap
extern "C" void* pycc_textwrap_fill(void* s, int32_t w) { return ::pycc::rt::textwrap_fill(s,w); }
extern "C" void* pycc_textwrap_shorten(void* s, int32_t w) { return ::pycc::rt::textwrap_shorten(s,w); }
extern "C" void* pycc_textwrap_wrap(void* s, int32_t w) { return ::pycc::rt::textwrap_wrap(s,w); }
extern "C" void* pycc_textwrap_dedent(void* s) { return ::pycc::rt::textwrap_dedent(s); }
namespace pycc::rt {

void* textwrap_indent(void* str, void* prefix) {
  if (!str || !prefix) return str;
  std::string s(string_data(str), string_len(str));
  std::string p(string_data(prefix), string_len(prefix));
  if (s.empty()) return string_from_cstr("");
  std::string out; out.reserve(s.size() + p.size() * 4);
  std::size_t i=0, n=s.size();
  while (i<n) {
    out += p;
    std::size_t j=i; while (j<n && s[j] != '\n') ++j;
    out.append(s, i, j-i);
    if (j<n && s[j]=='\n') { out.push_back('\n'); i = j+1; } else { i = j; }
  }
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

extern "C" void* pycc_textwrap_indent(void* s, void* p) { return ::pycc::rt::textwrap_indent(s,p); }

// ===== hashlib module (subset) =====
namespace pycc::rt {

static std::vector<unsigned char> to_bytes_any(void* obj) {
  std::vector<unsigned char> out;
  if (!obj) return out;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  TypeTag t = static_cast<TypeTag>(h->tag);
  if (t == TypeTag::Bytes) { const unsigned char* d = bytes_data(obj); out.assign(d, d + bytes_len(obj)); }
  else if (t == TypeTag::String) { const char* d = string_data(obj); out.assign(reinterpret_cast<const unsigned char*>(d), reinterpret_cast<const unsigned char*>(d) + string_len(obj)); }
  else if (t == TypeTag::Int) { long long v = box_int_value(obj); for (int i=0;i<8;++i) out.push_back(static_cast<unsigned char>((v >> (i*8)) & 0xFF)); }
  else if (t == TypeTag::Float) { double dv = box_float_value(obj); unsigned char buf[sizeof(double)]; std::memcpy(buf, &dv, sizeof(double)); out.insert(out.end(), buf, buf + sizeof(double)); }
  else if (t == TypeTag::Bool) { out.push_back(box_bool_value(obj) ? 1 : 0); }
  return out;
}

static uint64_t hash64_mix(const unsigned char* data, std::size_t len, uint64_t seed) {
  // FNV-1a 64-bit with a seed tweak
  const uint64_t FNV_OFFSET = 1469598103934665603ULL ^ seed;
  const uint64_t FNV_PRIME = 1099511628211ULL;
  uint64_t h = FNV_OFFSET;
  for (std::size_t i=0;i<len;++i) { h ^= static_cast<uint64_t>(data[i]); h *= FNV_PRIME; }
  // final avalanche (xorshift)
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;
  return h;
}

static std::string hex_from_u64(uint64_t v) {
  static const char* hex = "0123456789abcdef";
  std::string out; out.resize(16);
  for (int i=15; i>=0; --i) { out[i] = hex[v & 0xF]; v >>= 4; }
  return out;
}

void* hashlib_sha256(void* obj) {
  auto bytes = to_bytes_any(obj);
  // derive 4x64-bit blocks with different seeds
  uint64_t h0 = hash64_mix(bytes.data(), bytes.size(), 0x9ae16a3b2f90404fULL);
  uint64_t h1 = hash64_mix(bytes.data(), bytes.size(), 0xc3a5c85c97cb3127ULL);
  uint64_t h2 = hash64_mix(bytes.data(), bytes.size(), 0xb492b66fbe98f273ULL);
  uint64_t h3 = hash64_mix(bytes.data(), bytes.size(), 0x9ae16a3b2f90404dULL);
  std::string hex; hex.reserve(64);
  hex += hex_from_u64(h0); hex += hex_from_u64(h1); hex += hex_from_u64(h2); hex += hex_from_u64(h3);
  return string_new(hex.data(), hex.size());
}

void* hashlib_md5(void* obj) {
  auto bytes = to_bytes_any(obj);
  uint64_t h0 = hash64_mix(bytes.data(), bytes.size(), 0x0123456789abcdefULL);
  uint64_t h1 = hash64_mix(bytes.data(), bytes.size(), 0xfedcba9876543210ULL);
  std::string hex; hex.reserve(32);
  hex += hex_from_u64(h0); hex += hex_from_u64(h1);
  return string_new(hex.data(), hex.size());
}

} // namespace pycc::rt

// C ABI for hashlib
extern "C" void* pycc_hashlib_sha256(void* d) { return ::pycc::rt::hashlib_sha256(d); }
extern "C" void* pycc_hashlib_md5(void* d) { return ::pycc::rt::hashlib_md5(d); }

// ===== pprint module =====
namespace pycc::rt {

static std::string pformat_impl(void* obj, int depth);

static void append_escaped(std::string& out, const char* data, std::size_t n) {
  for (std::size_t i=0;i<n;++i) {
    char c = data[i];
    if (c == '\\' || c == '\'') { out.push_back('\\'); out.push_back(c); }
    else if (c == '\n') { out += "\\n"; }
    else out.push_back(c);
  }
}

static std::string pformat_impl(void* obj, int depth) {
  if (obj == nullptr) return std::string("None");
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int: return std::to_string(box_int_value(obj));
    case TypeTag::Float: {
      std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(12); ss<<box_float_value(obj); std::string s=ss.str();
      // strip trailing zeros
      while (s.size()>1 && s.find('.')!=std::string::npos && s.back()=='0') s.pop_back();
      if (!s.empty() && s.back()=='.') s.push_back('0');
      return s;
    }
    case TypeTag::Bool: return box_bool_value(obj) ? std::string("True") : std::string("False");
    case TypeTag::String: {
      const char* d = string_data(obj); std::size_t n = string_len(obj);
      std::string out; out.push_back('\''); append_escaped(out, d, n); out.push_back('\'');
      return out;
    }
    case TypeTag::List: {
      std::size_t n = list_len(obj);
      std::string out = "[";
      for (std::size_t i=0;i<n;++i) { if (i) out += ", "; out += pformat_impl(list_get(obj,i), depth+1); }
      out.push_back(']'); return out;
    }
    case TypeTag::Dict: {
      void* it = pycc_dict_iter_new(obj);
      std::string out = "{"; bool first=true; for (;;) { void* k = pycc_dict_iter_next(it); if (!k) break; void* v = dict_get(obj, k); if (!first) out += ", "; first=false; out += pformat_impl(k, depth+1); out += ": "; out += pformat_impl(v, depth+1); }
      out.push_back('}'); return out;
    }
    default: return std::string("<object>");
  }
}

void* pprint_pformat(void* obj) {
  std::string s = pformat_impl(obj, 0);
  return string_new(s.data(), s.size());
}

} // namespace pycc::rt

// C ABI for pprint
extern "C" void* pycc_pprint_pformat(void* o) { return ::pycc::rt::pprint_pformat(o); }

// ===== reprlib module (subset) =====
namespace pycc::rt {

void* reprlib_repr(void* obj) {
  // Use pprint formatting then truncate to a modest limit (60 chars)
  std::string s = pformat_impl(obj, 0);
  constexpr std::size_t kLimit = 60;
  if (s.size() > kLimit) {
    std::string out = s.substr(0, kLimit - 3);
    out += "...";
    return string_new(out.data(), out.size());
  }
  return string_new(s.data(), s.size());
}

} // namespace pycc::rt

// C ABI for reprlib
extern "C" void* pycc_reprlib_repr(void* o) { return ::pycc::rt::reprlib_repr(o); }

// ===== types module (subset) =====
namespace pycc::rt {

void* types_simple_namespace(void* list_of_pairs_opt) {
  void* obj = object_new(0);
  if (!list_of_pairs_opt) return obj;
  std::size_t n = list_len(list_of_pairs_opt);
  for (std::size_t i=0;i<n;++i) {
    void* pair = list_get(list_of_pairs_opt, i);
    if (!pair || list_len(pair) < 2) continue;
    void* k = list_get(pair, 0);
    void* v = list_get(pair, 1);
    if (!k) continue;
    // Ensure key is string
    void* keyStr = nullptr;
    auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(k) - sizeof(ObjectHeader));
    if (static_cast<TypeTag>(h->tag) == TypeTag::String) keyStr = k;
    else if (static_cast<TypeTag>(h->tag) == TypeTag::Int) {
      long long iv = box_int_value(k); std::string s = std::to_string(iv); keyStr = string_new(s.data(), s.size());
    } else if (static_cast<TypeTag>(h->tag) == TypeTag::Bool) {
      const char* s = box_bool_value(k) ? "True" : "False"; keyStr = string_from_cstr(s);
    } else { continue; }
    object_set_attr(obj, keyStr, v);
  }
  return obj;
}

} // namespace pycc::rt

// C ABI for types
extern "C" void* pycc_types_simple_namespace(void* pairs) { return ::pycc::rt::types_simple_namespace(pairs); }

// ===== linecache module =====
namespace pycc::rt {

void* linecache_getline(void* path, int32_t lineno) {
  if (!path || lineno <= 0) return string_from_cstr("");
  const char* p = string_data(path);
  if (!p) return string_from_cstr("");
  void* content = io_read_file(p);
  if (!content) return string_from_cstr("");
  const char* d = string_data(content); std::size_t n = string_len(content);
  int32_t cur = 1;
  std::size_t i = 0; std::size_t start = 0; bool found=false;
  while (i < n) {
    if (cur == lineno) { start = i; found = true; break; }
    if (d[i] == '\n') { ++cur; }
    ++i;
  }
  if (!found) return string_from_cstr("");
  std::size_t j = start;
  while (j < n && d[j] != '\n') ++j;
  return string_new(d + start, j - start);
}

} // namespace pycc::rt

// C ABI for linecache
extern "C" void* pycc_linecache_getline(void* p, int32_t n) { return ::pycc::rt::linecache_getline(p,n); }

// ===== getpass module =====
namespace pycc::rt {

void* getpass_getuser() {
  const char* envs[] = {"USER", "USERNAME"};
  for (const char* e : envs) { const char* v = std::getenv(e); if (v && *v) return string_from_cstr(v); }
  return string_from_cstr("unknown");
}

void* getpass_getpass(void* /*prompt_opt*/) {
  return string_from_cstr("");
}

} // namespace pycc::rt

// C ABI for getpass
extern "C" void* pycc_getpass_getuser() { return ::pycc::rt::getpass_getuser(); }
extern "C" void* pycc_getpass_getpass(void* p) { return ::pycc::rt::getpass_getpass(p); }

// ===== shlex module =====
namespace pycc::rt {

static void shlex_emit_token(std::vector<std::string>& out, std::string& cur) {
  if (!cur.empty()) { out.push_back(cur); cur.clear(); }
}

void* shlex_split(void* str) {
  if (!str) return list_new(0);
  const char* d = string_data(str); std::size_t n = string_len(str);
  std::vector<std::string> toks; toks.reserve(8);
  std::string cur;
  bool in_s=false, in_d=false, esc=false;
  for (std::size_t i=0;i<n;++i) {
    char c = d[i];
    if (esc) { cur.push_back(c); esc=false; continue; }
    if (c == '\\') { esc=true; continue; }
    if (in_s) { if (c=='\'') { in_s=false; } else { cur.push_back(c); } continue; }
    if (in_d) { if (c=='"') { in_d=false; } else { cur.push_back(c); } continue; }
    if (c=='\'') { in_s=true; continue; }
    if (c=='"') { in_d=true; continue; }
    if (std::isspace(static_cast<unsigned char>(c))) { shlex_emit_token(toks, cur); continue; }
    cur.push_back(c);
  }
  shlex_emit_token(toks, cur);
  void* lst = list_new(toks.size());
  for (const auto& t : toks) { list_push_slot(&lst, string_new(t.data(), t.size())); }
  return lst;
}

void* shlex_join(void* list_of_strings) {
  if (!list_of_strings) return string_from_cstr("");
  std::string out; bool first=true;
  const std::size_t n = list_len(list_of_strings);
  for (std::size_t i=0;i<n;++i) {
    void* s = list_get(list_of_strings, i);
    const char* d = s ? string_data(s) : nullptr; std::size_t len = s ? string_len(s) : 0;
    std::string v = d ? std::string(d, len) : std::string();
    bool needQuote=false;
    for (char c : v) { if (std::isspace(static_cast<unsigned char>(c)) || c=='"' || c=='\'' || c=='$' || c=='`' || c=='\\') { needQuote=true; break; } }
    if (!first) out.push_back(' ');
    if (!needQuote) out += v;
    else {
      out.push_back('\'');
      for (char c : v) {
        if (c=='\'') { out += "'\\''"; }
        else out.push_back(c);
      }
      out.push_back('\'');
    }
    first=false;
  }
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for shlex
extern "C" void* pycc_shlex_split(void* s) { return ::pycc::rt::shlex_split(s); }
extern "C" void* pycc_shlex_join(void* l) { return ::pycc::rt::shlex_join(l); }

// ===== html module =====
namespace pycc::rt {

void* html_escape(void* str, int32_t quote) {
  if (!str) return string_from_cstr("");
  const char* d = string_data(str); std::size_t n = string_len(str);
  std::string out; out.reserve(n);
  bool q = (quote != 0);
  for (std::size_t i=0;i<n;++i) {
    char c = d[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (q && c == '"') out += "&quot;";
    else if (q && c == '\'') out += "&#x27;";
    else out.push_back(c);
  }
  return string_new(out.data(), out.size());
}

// (from_hex_digit moved to HTML unescape handler)

void* html_unescape(void* str) {
  if (!str) return string_from_cstr("");
  const char* d = string_data(str); std::size_t n = string_len(str);
  std::string out; detail::html_unescape_impl(d, n, out);
  return string_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for html
extern "C" void* pycc_html_escape(void* s, int32_t q) { return ::pycc::rt::html_escape(s,q); }
extern "C" void* pycc_html_unescape(void* s) { return ::pycc::rt::html_unescape(s); }

// ===== binascii module =====
namespace pycc::rt {

static std::vector<unsigned char> to_bytes_for_binascii(void* obj) {
  std::vector<unsigned char> out;
  if (!obj) return out;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  TypeTag t = static_cast<TypeTag>(h->tag);
  if (t == TypeTag::Bytes) {
    const unsigned char* d = bytes_data(obj); out.assign(d, d + bytes_len(obj));
  } else if (t == TypeTag::String) {
    const char* d = string_data(obj); out.assign(reinterpret_cast<const unsigned char*>(d), reinterpret_cast<const unsigned char*>(d) + string_len(obj));
  }
  return out;
}

void* binascii_hexlify(void* data) {
  auto in = to_bytes_for_binascii(data);
  static const char* hex = "0123456789abcdef";
  std::string out; out.resize(in.size() * 2);
  for (std::size_t i=0;i<in.size();++i) { unsigned char v = in[i]; out[2*i] = hex[(v>>4)&0xF]; out[2*i+1] = hex[v&0xF]; }
  return bytes_new(out.data(), out.size());
}

void* binascii_unhexlify(void* data) {
  auto in = to_bytes_for_binascii(data);
  auto val = [](unsigned char c)->int{ if (c>='0'&&c<='9') return c-'0'; if (c>='a'&&c<='f') return c-'a'+10; if (c>='A'&&c<='F') return c-'A'+10; return -1; };
  std::string out; out.reserve(in.size()/2);
  std::size_t i=0;
  // skip optional 0x prefix
  if (in.size()>=2 && in[0]=='0' && (in[1]=='x' || in[1]=='X')) i=2;
  for (; i+1<in.size(); i+=2) {
    int hi = val(in[i]); int lo = val(in[i+1]); if (hi<0 || lo<0) break; out.push_back(static_cast<char>((hi<<4)|lo));
  }
  return bytes_new(out.data(), out.size());
}

} // namespace pycc::rt

// C ABI for binascii
extern "C" void* pycc_binascii_hexlify(void* d) { return ::pycc::rt::binascii_hexlify(d); }
extern "C" void* pycc_binascii_unhexlify(void* d) { return ::pycc::rt::binascii_unhexlify(d); }

// ===== hmac module =====
namespace pycc::rt {

static std::vector<unsigned char> to_bytes_any2(void* obj) {
  std::vector<unsigned char> out;
  if (!obj) return out;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  TypeTag t = static_cast<TypeTag>(h->tag);
  if (t == TypeTag::Bytes) { const unsigned char* d = bytes_data(obj); out.assign(d, d + bytes_len(obj)); }
  else if (t == TypeTag::String) { const char* d = string_data(obj); out.assign(reinterpret_cast<const unsigned char*>(d), reinterpret_cast<const unsigned char*>(d) + string_len(obj)); }
  else if (t == TypeTag::Bool) { out.push_back(box_bool_value(obj) ? 1 : 0); }
  else if (t == TypeTag::Int) { long long v = box_int_value(obj); for (int i=0;i<8;++i) out.push_back(static_cast<unsigned char>((v >> (i*8)) & 0xFF)); }
  else if (t == TypeTag::Float) { double dv = box_float_value(obj); unsigned char buf[sizeof(double)]; std::memcpy(buf, &dv, sizeof(double)); out.insert(out.end(), buf, buf+sizeof(double)); }
  return out;
}

static void* hash_hex_to_bytes(void* hexStr) {
  return binascii_unhexlify(hexStr);
}

void* hmac_digest(void* keyObj, void* msgObj, void* digestmodObj) {
  std::vector<unsigned char> key = to_bytes_any2(keyObj);
  std::vector<unsigned char> msg = to_bytes_any2(msgObj);
  std::string algo;
  if (digestmodObj && string_len(digestmodObj) > 0) algo.assign(string_data(digestmodObj), string_len(digestmodObj)); else algo = "sha256";
  int block = 64; // for sha256/md5
  // Shorten long key
  if (key.size() > static_cast<std::size_t>(block)) {
    void* kstr = string_new(reinterpret_cast<const char*>(key.data()), key.size());
    void* hhex = (algo == "md5") ? hashlib_md5(kstr) : hashlib_sha256(kstr);
    void* kbytes = hash_hex_to_bytes(hhex);
    key.assign(bytes_data(kbytes), bytes_data(kbytes) + bytes_len(kbytes));
  }
  // Pad key to block
  key.resize(static_cast<std::size_t>(block), 0);
  std::vector<unsigned char> kipad(block), kopad(block);
  for (int i=0;i<block;++i) { kipad[i] = key[static_cast<std::size_t>(i)] ^ 0x36U; kopad[i] = key[static_cast<std::size_t>(i)] ^ 0x5cU; }
  // inner = H(k_ipad || msg)
  std::string inner; inner.reserve(block + msg.size()); inner.append(reinterpret_cast<const char*>(kipad.data()), block); inner.append(reinterpret_cast<const char*>(msg.data()), msg.size());
  void* innerStr = string_new(inner.data(), inner.size());
  void* innerHex = (algo == "md5") ? hashlib_md5(innerStr) : hashlib_sha256(innerStr);
  void* innerBytes = hash_hex_to_bytes(innerHex);
  // outer = H(k_opad || inner)
  std::string outer; outer.reserve(block + bytes_len(innerBytes)); outer.append(reinterpret_cast<const char*>(kopad.data()), block); outer.append(reinterpret_cast<const char*>(bytes_data(innerBytes)), bytes_len(innerBytes));
  void* outerStr = string_new(outer.data(), outer.size());
  void* outHex = (algo == "md5") ? hashlib_md5(outerStr) : hashlib_sha256(outerStr);
  void* outBytes = hash_hex_to_bytes(outHex);
  return outBytes;
}

} // namespace pycc::rt

// C ABI for hmac
extern "C" void* pycc_hmac_digest(void* k, void* m, void* a) { return ::pycc::rt::hmac_digest(k,m,a); }

// ===== warnings module =====
namespace pycc::rt {

void warnings_warn(void* msg) {
  if (msg) {
    io_write_stderr(msg);
    io_write_stderr(string_from_cstr("\n"));
  }
}

void warnings_simplefilter(void* /*action*/, void* /*category_opt*/) { /* no-op */ }

} // namespace pycc::rt

// C ABI for warnings
extern "C" void pycc_warnings_warn(void* s) { ::pycc::rt::warnings_warn(s); }
extern "C" void pycc_warnings_simplefilter(void* a, void* c) { ::pycc::rt::warnings_simplefilter(a,c); }

// ===== copy module =====
namespace pycc::rt {

static void* shallow_copy_obj(void* obj);
static void* deep_copy_obj(void* obj);

static void* shallow_copy_obj(void* obj) {
  if (!obj) return nullptr;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int:
    case TypeTag::Float:
    case TypeTag::Bool:
    case TypeTag::String:
    case TypeTag::Bytes:
      return obj; // immutable
    case TypeTag::List: {
      std::size_t n = list_len(obj);
      void* out = list_new(n);
      for (std::size_t i=0;i<n;++i) { list_push_slot(&out, list_get(obj,i)); }
      return out;
    }
    case TypeTag::Dict: {
      void* out = dict_new(8);
      void* it = pycc_dict_iter_new(obj);
      for (;;) { void* k = pycc_dict_iter_next(it); if (!k) break; void* v = dict_get(obj, k); dict_set(&out, k, v); }
      return out;
    }
    default: return obj;
  }
}

static void* deep_copy_obj(void* obj) {
  if (!obj) return nullptr;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int:
    case TypeTag::Float:
    case TypeTag::Bool:
    case TypeTag::String:
    case TypeTag::Bytes:
      return obj;
    case TypeTag::List: {
      std::size_t n = list_len(obj);
      void* out = list_new(n);
      for (std::size_t i=0;i<n;++i) { list_push_slot(&out, deep_copy_obj(list_get(obj,i))); }
      return out;
    }
    case TypeTag::Dict: {
      void* out = dict_new(8);
      void* it = pycc_dict_iter_new(obj);
      for (;;) { void* k = pycc_dict_iter_next(it); if (!k) break; void* v = dict_get(obj, k); dict_set(&out, deep_copy_obj(k), deep_copy_obj(v)); }
      return out;
    }
    default: return obj;
  }
}

void* copy_copy(void* obj) { return shallow_copy_obj(obj); }
void* copy_deepcopy(void* obj) { return deep_copy_obj(obj); }

} // namespace pycc::rt

// C ABI for copy
extern "C" void* pycc_copy_copy(void* o) { return ::pycc::rt::copy_copy(o); }
extern "C" void* pycc_copy_deepcopy(void* o) { return ::pycc::rt::copy_deepcopy(o); }

// ===== calendar module =====
namespace pycc::rt {

static bool leap_year(int y) { return (y%4==0 && y%100!=0) || (y%400==0); }

int32_t calendar_isleap(int32_t year) { return leap_year(year) ? 1 : 0; }

static int days_in_month(int y, int m) {
  static const int dm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m==2) return dm[m-1] + (leap_year(y)?1:0); else return dm[m-1];
}

// Zeller-like: compute weekday with Monday=0..Sunday=6
static int weekday_mon0(int y, int m, int d) {
  // Tomohiko Sakamoto's algorithm yields 0=Sunday..6=Saturday
  static int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (m < 3) y -= 1;
  int w = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7; // 0=Sunday
  int mon0 = (w + 6) % 7; // Monday=0
  return mon0;
}

void* calendar_monthrange(int32_t year, int32_t month) {
  if (month < 1 || month > 12) return list_new(0);
  int wd = weekday_mon0(year, month, 1);
  int nd = days_in_month(year, month);
  void* lst = list_new(2);
  list_push_slot(&lst, box_int(wd));
  list_push_slot(&lst, box_int(nd));
  return lst;
}

} // namespace pycc::rt

// C ABI for calendar
extern "C" int32_t pycc_calendar_isleap(int32_t y) { return ::pycc::rt::calendar_isleap(y); }
extern "C" void* pycc_calendar_monthrange(int32_t y, int32_t m) { return ::pycc::rt::calendar_monthrange(y,m); }

// ---- keyword module shims ----
namespace pycc::rt {

static inline std::string_view kw_to_sv(void* s) {
  if (!s) return std::string_view{};
  const char* d = string_data(s);
  return std::string_view{d, string_len(s)};
}

bool keyword_iskeyword(void* s) {
  static const std::array<std::string_view, 37> kws = {
      std::string_view{"False"}, std::string_view{"None"}, std::string_view{"True"},
      std::string_view{"and"}, std::string_view{"as"}, std::string_view{"assert"},
      std::string_view{"async"}, std::string_view{"await"}, std::string_view{"break"},
      std::string_view{"case"}, std::string_view{"class"}, std::string_view{"continue"},
      std::string_view{"def"}, std::string_view{"del"}, std::string_view{"elif"},
      std::string_view{"else"}, std::string_view{"except"}, std::string_view{"finally"},
      std::string_view{"for"}, std::string_view{"from"}, std::string_view{"global"},
      std::string_view{"if"}, std::string_view{"import"}, std::string_view{"in"},
      std::string_view{"is"}, std::string_view{"lambda"}, std::string_view{"match"},
      std::string_view{"nonlocal"}, std::string_view{"not"}, std::string_view{"or"},
      std::string_view{"pass"}, std::string_view{"raise"}, std::string_view{"return"},
      std::string_view{"try"}, std::string_view{"while"}, std::string_view{"with"},
      std::string_view{"yield"}
  };
  const auto v = kw_to_sv(s);
  for (auto k : kws) { if (v == k) return true; }
  return false;
}

void* keyword_kwlist() {
  static const std::array<const char*, 37> kws = {
      "False","None","True","and","as","assert","async","await","break","case","class","continue","def","del","elif","else","except","finally","for","from","global","if","import","in","is","lambda","match","nonlocal","not","or","pass","raise","return","try","while","with","yield"
  };
  void* lst = list_new(kws.size());
  for (const char* kw : kws) {
    void* s = string_from_cstr(kw);
    list_push_slot(&lst, s);
  }
  return lst;
}

} // namespace pycc::rt

// C ABI wrappers for keyword
extern "C" int  pycc_keyword_iskeyword(void* s) { return ::pycc::rt::keyword_iskeyword(s) ? 1 : 0; }
extern "C" void* pycc_keyword_kwlist() { return ::pycc::rt::keyword_kwlist(); }

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

// ===== operator module =====
namespace pycc::rt {
static TypeTag runtime_type_of(void* obj) {
  if (!obj) return TypeTag::Object;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(obj) - sizeof(ObjectHeader));
  return static_cast<TypeTag>(h->tag);
}
static bool is_num_tag(TypeTag t) { return t == TypeTag::Int || t == TypeTag::Float || t == TypeTag::Bool; }
static inline double to_double_num(void* a) {
  switch (runtime_type_of(a)) {
    case TypeTag::Int: return static_cast<double>(box_int_value(a));
    case TypeTag::Float: return box_float_value(a);
    case TypeTag::Bool: return box_bool_value(a) ? 1.0 : 0.0;
    default: return 0.0;
  }
}
static inline long long to_ll_num(void* a) {
  switch (runtime_type_of(a)) {
    case TypeTag::Int: return box_int_value(a);
    case TypeTag::Bool: return box_bool_value(a) ? 1LL : 0LL;
    default: return 0LL;
  }
}
void* operator_add(void* a, void* b) {
  TypeTag ta = runtime_type_of(a), tb = runtime_type_of(b);
  if (ta == TypeTag::Float || tb == TypeTag::Float) return box_float(to_double_num(a) + to_double_num(b));
  long long r = to_ll_num(a) + to_ll_num(b); return box_int(r);
}
void* operator_sub(void* a, void* b) {
  TypeTag ta = runtime_type_of(a), tb = runtime_type_of(b);
  if (ta == TypeTag::Float || tb == TypeTag::Float) return box_float(to_double_num(a) - to_double_num(b));
  long long r = to_ll_num(a) - to_ll_num(b); return box_int(r);
}
void* operator_mul(void* a, void* b) {
  TypeTag ta = runtime_type_of(a), tb = runtime_type_of(b);
  if (ta == TypeTag::Float || tb == TypeTag::Float) return box_float(to_double_num(a) * to_double_num(b));
  long long r = to_ll_num(a) * to_ll_num(b); return box_int(r);
}
void* operator_truediv(void* a, void* b) { return box_float(to_double_num(a) / to_double_num(b)); }
void* operator_neg(void* a) {
  return (runtime_type_of(a) == TypeTag::Float) ? box_float(-box_float_value(a)) : box_int(-to_ll_num(a));
}
bool operator_eq(void* a, void* b) {
  TypeTag ta = runtime_type_of(a), tb = runtime_type_of(b);
  if (is_num_tag(ta) && is_num_tag(tb)) return to_double_num(a) == to_double_num(b);
  if (ta == TypeTag::String && tb == TypeTag::String) {
    const char* sa = string_data(a); const char* sb = string_data(b);
    std::size_t la = string_len(a), lb = string_len(b);
    if (la != lb) return false; return std::memcmp(sa, sb, la) == 0;
  }
  return a == b;
}
bool operator_lt(void* a, void* b) {
  TypeTag ta = runtime_type_of(a), tb = runtime_type_of(b);
  if (is_num_tag(ta) && is_num_tag(tb)) return to_double_num(a) < to_double_num(b);
  if (ta == TypeTag::String && tb == TypeTag::String) { return std::strcmp(string_data(a), string_data(b)) < 0; }
  return false;
}
bool operator_not_(void* a) { return !operator_truth(a); }
bool operator_truth(void* a) {
  switch (runtime_type_of(a)) {
    case TypeTag::Bool: return box_bool_value(a);
    case TypeTag::Int: return box_int_value(a) != 0;
    case TypeTag::Float: return box_float_value(a) != 0.0;
    case TypeTag::String: return string_len(a) != 0;
    case TypeTag::List: return list_len(a) != 0;
    case TypeTag::Dict: return dict_len(a) != 0;
    default: return a != nullptr;
  }
}
} // namespace pycc::rt

// C ABI for operator
extern "C" void* pycc_operator_add(void* a, void* b) { return ::pycc::rt::operator_add(a,b); }
extern "C" void* pycc_operator_sub(void* a, void* b) { return ::pycc::rt::operator_sub(a,b); }
extern "C" void* pycc_operator_mul(void* a, void* b) { return ::pycc::rt::operator_mul(a,b); }
extern "C" void* pycc_operator_truediv(void* a, void* b) { return ::pycc::rt::operator_truediv(a,b); }
extern "C" void* pycc_operator_neg(void* a) { return ::pycc::rt::operator_neg(a); }
extern "C" int  pycc_operator_eq(void* a, void* b) { return ::pycc::rt::operator_eq(a,b) ? 1 : 0; }
extern "C" int  pycc_operator_lt(void* a, void* b) { return ::pycc::rt::operator_lt(a,b) ? 1 : 0; }
extern "C" int  pycc_operator_not(void* a) { return ::pycc::rt::operator_not_(a) ? 1 : 0; }
extern "C" int  pycc_operator_truth(void* a) { return ::pycc::rt::operator_truth(a) ? 1 : 0; }
// ===== collections module =====
namespace pycc::rt {

void* collections_counter(void* iterable_list) {
  void* d = dict_new(8);
  if (iterable_list == nullptr) return d;
  const std::size_t n = list_len(iterable_list);
  std::unordered_map<long long, void*> intCache;
  for (std::size_t i = 0; i < n; ++i) {
    void* k = list_get(iterable_list, i);
    // Canonicalize key: use string as-is; for ints use decimal string
    void* key = k;
    if (k != nullptr) {
      auto* hdr = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(k) - sizeof(ObjectHeader));
      if (static_cast<TypeTag>(hdr->tag) == TypeTag::Int) {
        long long v = box_int_value(k);
        auto it = intCache.find(v);
        if (it != intCache.end()) { key = it->second; }
        else {
          std::string s = std::to_string(v);
          void* ks = string_new(s.data(), s.size());
          intCache.emplace(v, ks);
          key = ks;
        }
      }
    }
    void* cur = dict_get(d, key);
    long long v = cur ? box_int_value(cur) : 0;
    void* next = box_int(v + 1);
    dict_set(&d, key, next);
  }
  return d;
}

void* collections_ordered_dict(void* list_of_pairs) {
  void* d = dict_new(8);
  if (list_of_pairs == nullptr) return d;
  const std::size_t n = list_len(list_of_pairs);
  for (std::size_t i = 0; i < n; ++i) {
    void* pair = list_get(list_of_pairs, i);
    if (pair == nullptr || list_len(pair) < 2) continue;
    void* k = list_get(pair, 0);
    void* v = list_get(pair, 1);
    dict_set(&d, k, v);
  }
  return d;
}

void* collections_chainmap(void* list_of_dicts) {
  // Merge left-to-right, first dict has precedence.
  void* out = dict_new(8);
  if (list_of_dicts == nullptr) return out;
  const std::size_t n = list_len(list_of_dicts);
  // Iterate from right to left so leftmost takes precedence.
  for (std::size_t ri = 0; ri < n; ++ri) {
    std::size_t i = (n - 1) - ri;
    void* d = list_get(list_of_dicts, i);
    // Iterate dict via iterator API
    void* it = pycc_dict_iter_new(d);
    while (true) {
      void* k = pycc_dict_iter_next(it);
      if (k == nullptr) break;
      void* v = dict_get(d, k);
      dict_set(&out, k, v);
    }
  }
  return out;
}

void* collections_defaultdict_new(void* default_value) {
  void* dd = object_new(2);
  object_set(dd, 0, dict_new(8));
  object_set(dd, 1, default_value);
  return dd;
}

static inline void* dd_dict(void* dd) {
  auto* meta = reinterpret_cast<std::size_t*>(dd);
  auto** vals = reinterpret_cast<void**>(meta + 1);
  return vals[0];
}
static inline void* dd_default(void* dd) {
  auto* meta = reinterpret_cast<std::size_t*>(dd);
  auto** vals = reinterpret_cast<void**>(meta + 1);
  return vals[1];
}

void* collections_defaultdict_get(void* dd, void* key) {
  if (dd == nullptr) return nullptr;
  void* d = dd_dict(dd);
  void* v = dict_get(d, key);
  if (v != nullptr) return v;
  void* defv = dd_default(dd);
  dict_set(&d, key, defv);
  // write back dictionary in case dict_set replaced it (it doesn't, but keep consistent)
  object_set(dd, 0, d);
  return defv;
}

void collections_defaultdict_set(void* dd, void* key, void* value) {
  if (dd == nullptr) return;
  void* d = dd_dict(dd);
  dict_set(&d, key, value);
  object_set(dd, 0, d);
}

} // namespace pycc::rt

extern "C" void* pycc_collections_counter(void* lst) { return ::pycc::rt::collections_counter(lst); }
extern "C" void* pycc_collections_ordered_dict(void* pairs) { return ::pycc::rt::collections_ordered_dict(pairs); }
extern "C" void* pycc_collections_chainmap(void* dicts) { return ::pycc::rt::collections_chainmap(dicts); }
extern "C" void* pycc_collections_defaultdict_new(void* defv) { return ::pycc::rt::collections_defaultdict_new(defv); }
extern "C" void* pycc_collections_defaultdict_get(void* dd, void* key) { return ::pycc::rt::collections_defaultdict_get(dd,key); }
extern "C" void  pycc_collections_defaultdict_set(void* dd, void* key, void* val) { ::pycc::rt::collections_defaultdict_set(dd,key,val); }

// ===== array module (minimal) =====
namespace pycc::rt {

static inline char array_typecode(void* arr) {
  if (!arr) return 'i';
  void* tc = object_get(arr, 0);
  if (!tc) return 'i';
  const char* data = string_data(tc);
  std::size_t len = string_len(tc);
  return len > 0 ? data[0] : 'i';
}
static inline void* array_storage(void* arr) { return object_get(arr, 1); }
static inline void  array_set_storage(void* arr, void* lst) { object_set(arr, 1, lst); }

static inline long long to_int_like_any(void* v) {
  if (!v) return 0;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(v) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int: return box_int_value(v);
    case TypeTag::Float: return static_cast<long long>(box_float_value(v));
    case TypeTag::Bool: return box_bool_value(v) ? 1 : 0;
    default: return 0;
  }
}
static inline double to_float_like_any(void* v) {
  if (!v) return 0.0;
  auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(v) - sizeof(ObjectHeader));
  switch (static_cast<TypeTag>(h->tag)) {
    case TypeTag::Int: return static_cast<double>(box_int_value(v));
    case TypeTag::Float: return box_float_value(v);
    case TypeTag::Bool: return box_bool_value(v) ? 1.0 : 0.0;
    default: return 0.0;
  }
}

void* array_array(void* typecode_str, void* initializer_list_or_null) {
  void* arr = object_new(2);
  if (!typecode_str) typecode_str = string_from_cstr("i");
  object_set(arr, 0, typecode_str);
  void* storage = list_new(0);
  if (initializer_list_or_null) {
    std::size_t n = list_len(initializer_list_or_null);
    for (std::size_t i=0;i<n;++i) {
      void* el = list_get(initializer_list_or_null, i);
      char tc = array_typecode(arr);
      if (tc == 'f') {
        double dv = to_float_like_any(el);
        list_push_slot(&storage, box_float(dv));
      } else {
        long long iv = to_int_like_any(el);
        if (tc == 'b') {
          if (iv < -128) iv = -128; if (iv > 127) iv = 127;
        }
        list_push_slot(&storage, box_int(iv));
      }
    }
  }
  array_set_storage(arr, storage);
  return arr;
}

void array_append(void* arr, void* value) {
  if (!arr) return;
  void* storage = array_storage(arr);
  char tc = array_typecode(arr);
  if (tc == 'f') {
    double dv = to_float_like_any(value);
    list_push_slot(&storage, box_float(dv));
  } else {
    long long iv = to_int_like_any(value);
    if (tc == 'b') { if (iv < -128) iv = -128; if (iv > 127) iv = 127; }
    list_push_slot(&storage, box_int(iv));
  }
  array_set_storage(arr, storage);
}

void* array_pop(void* arr) {
  if (!arr) return nullptr;
  void* storage = array_storage(arr);
  if (!storage) return nullptr;
  auto* meta = reinterpret_cast<std::size_t*>(storage);
  std::size_t n = meta[0];
  if (n == 0) return nullptr;
  auto** items = reinterpret_cast<void**>(meta + 2);
  void* last = items[n - 1];
  gc_pre_barrier(&items[n - 1]); items[n - 1] = nullptr; gc_write_barrier(&items[n - 1], nullptr);
  meta[0] = n - 1;
  return last;
}

void* array_tolist(void* arr) {
  if (!arr) return list_new(0);
  void* storage = array_storage(arr);
  if (!storage) return list_new(0);
  std::size_t n = list_len(storage);
  void* out = list_new(n);
  for (std::size_t i=0;i<n;++i) { list_push_slot(&out, list_get(storage, i)); }
  return out;
}

} // namespace pycc::rt

// C ABI for array
extern "C" void* pycc_array_array(void* tc, void* init) { return ::pycc::rt::array_array(tc, init); }
extern "C" void  pycc_array_append(void* arr, void* v) { ::pycc::rt::array_append(arr, v); }
extern "C" void* pycc_array_pop(void* arr) { return ::pycc::rt::array_pop(arr); }
extern "C" void* pycc_array_tolist(void* arr) { return ::pycc::rt::array_tolist(arr); }

// ===== unicodedata module (subset) =====
namespace pycc::rt {

void* unicodedata_normalize(void* form_str, void* s_str) {
  if (!form_str || !s_str) return s_str;
  std::string f(string_data(form_str), string_len(form_str));
  NormalizationForm form = NormalizationForm::NFC;
  if (f == "NFC") form = NormalizationForm::NFC;
  else if (f == "NFD") form = NormalizationForm::NFD;
  else if (f == "NFKC") form = NormalizationForm::NFKC;
  else if (f == "NFKD") form = NormalizationForm::NFKD;
  else { rt_raise("ValueError", "unicodedata.normalize: unknown form"); return nullptr; }
  return string_normalize(s_str, form);
}

} // namespace pycc::rt

// C ABI for unicodedata
extern "C" void* pycc_unicodedata_normalize(void* form, void* s) { return ::pycc::rt::unicodedata_normalize(form, s); }

// ===== struct module (subset) =====
namespace pycc::rt {

struct FmtItem { char code; int count; };

static bool parse_struct_fmt(const std::string& fmt, std::vector<FmtItem>& out, bool& little) {
  little = true; // default little-endian in this subset
  std::size_t i = 0, n = fmt.size();
  if (i<n && (fmt[i]=='<' || fmt[i]=='>')) { little = (fmt[i] == '<'); ++i; }
  while (i < n) {
    int count = 0;
    while (i<n && std::isdigit(static_cast<unsigned char>(fmt[i]))) { count = count*10 + (fmt[i]-'0'); ++i; }
    if (count == 0) count = 1;
    if (i>=n) return false;
    char c = fmt[i++];
    if (!(c=='i' || c=='I' || c=='b' || c=='B' || c=='f')) return false;
    out.push_back(FmtItem{c, count});
  }
  return true;
}

// append_u32/read_u32 moved to detail helpers

void* struct_pack(void* fmt_str, void* values_list) {
  if (!fmt_str) return bytes_new(nullptr, 0);
  std::string fmt(string_data(fmt_str), string_len(fmt_str));
  std::vector<FmtItem> items; bool little;
  if (!parse_struct_fmt(fmt, items, little)) { rt_raise("ValueError", "struct.pack: invalid format"); return nullptr; }
  std::vector<detail::StructItem> parsed; parsed.reserve(items.size());
  for (const auto& it : items) parsed.push_back(detail::StructItem{it.code, it.count});
  std::vector<unsigned char> out; out.reserve(16);
  detail::struct_pack_impl(parsed, little, values_list, out);
  if (rt_has_exception()) return nullptr;
  return bytes_new(out.data(), out.size());
}

void* struct_unpack(void* fmt_str, void* data_bytes) {
  if (!fmt_str || !data_bytes) return list_new(0);
  std::string fmt(string_data(fmt_str), string_len(fmt_str));
  std::vector<FmtItem> items; bool little;
  if (!parse_struct_fmt(fmt, items, little)) { rt_raise("ValueError", "struct.unpack: invalid format"); return nullptr; }
  const unsigned char* p = bytes_data(data_bytes);
  std::size_t nb = bytes_len(data_bytes);
  std::size_t need = 0; for (const auto& it: items){ int w=(it.code=='f'||it.code=='i'||it.code=='I')?4:1; need += static_cast<std::size_t>(it.count)*w; }
  if (nb != need) { rt_raise("ValueError", "struct.unpack: wrong size"); return nullptr; }
  std::vector<detail::StructItem> parsed; parsed.reserve(items.size());
  for (const auto& it : items) parsed.push_back(detail::StructItem{it.code, it.count});
  void* out = list_new(0);
  detail::struct_unpack_impl(parsed, little, p, nb, out);
  return out;
}

int32_t struct_calcsize(void* fmt_str) {
  if (!fmt_str) return 0;
  std::string fmt(string_data(fmt_str), string_len(fmt_str));
  std::vector<FmtItem> items; bool little;
  if (!parse_struct_fmt(fmt, items, little)) return 0;
  std::vector<detail::StructItem> parsed; parsed.reserve(items.size());
  for (const auto& it : items) parsed.push_back(detail::StructItem{it.code, it.count});
  return detail::struct_calcsize_impl(parsed);
}

} // namespace pycc::rt

// C ABI for struct
extern "C" void* pycc_struct_pack(void* fmt, void* vals) { return ::pycc::rt::struct_pack(fmt, vals); }
extern "C" void* pycc_struct_unpack(void* fmt, void* data) { return ::pycc::rt::struct_unpack(fmt, data); }
extern "C" int  pycc_struct_calcsize(void* fmt) { return ::pycc::rt::struct_calcsize(fmt); }

// ===== argparse module (subset) =====
namespace pycc::rt {

// Parser object layout: [0] = dict opt_to_name, [1] = dict name_to_action

void* argparse_argument_parser() {
  void* p = object_new(2);
  object_set(p, 0, dict_new(8));
  object_set(p, 1, dict_new(8));
  return p;
}

static inline void* ap_opt_map(void* p){ return object_get(p, 0); }
static inline void* ap_act_map(void* p){ return object_get(p, 1); }

static std::string canonical_name(const std::string& flag) {
  std::size_t i = 0; while (i<flag.size() && flag[i]=='-') ++i;
  if (i>=flag.size()) return std::string();
  return flag.substr(i);
}

void argparse_add_argument(void* parser, void* name_str, void* action_str) {
  if (!parser || !name_str || !action_str) return;
  std::string names(string_data(name_str), string_len(name_str));
  std::string action(string_data(action_str), string_len(action_str));
  void* optmap = ap_opt_map(parser);
  void* actmap = ap_act_map(parser);
  // Split names on '|'
  std::size_t start = 0;
  std::string firstName;
  while (start <= names.size()) {
    std::size_t sep = names.find('|', start);
    std::string token = (sep == std::string::npos) ? names.substr(start) : names.substr(start, sep - start);
    if (!token.empty()) {
      std::string canon = canonical_name(token);
      if (firstName.empty()) firstName = canon;
      void* tokS = string_new(token.data(), token.size());
      void* canonS = string_new(canon.data(), canon.size());
      dict_set(&optmap, tokS, canonS);
    }
    if (sep == std::string::npos) break; else start = sep + 1;
  }
  // Record action by canonical name
  if (!firstName.empty()) {
    void* key = string_new(firstName.data(), firstName.size());
    void* act = string_new(action.data(), action.size());
    dict_set(&actmap, key, act);
  }
  object_set(parser, 0, optmap);
  object_set(parser, 1, actmap);
}

void* argparse_parse_args(void* parser, void* args_list) {
  if (!parser) return dict_new(0);
  void* result = dict_new(8);
  void* optmap = ap_opt_map(parser);
  void* actmap = ap_act_map(parser);
  const std::size_t n = (args_list ? list_len(args_list) : 0);
  for (std::size_t i = 0; i < n; ++i) {
    void* tok = list_get(args_list, i);
    if (!tok) continue;
    std::string t(string_data(tok), string_len(tok));
    if (t.empty() || t[0] != '-') continue;
    detail::OptVal ov = detail::argparse_split_optval(t);
    void* canon = detail::argparse_lookup_canon(optmap, ov.opt);
    if (!canon) continue;
    void* act = dict_get(actmap, canon);
    if (!act) continue;
    std::string an(string_data(act), string_len(act));
    if (!detail::argparse_apply_action(an, canon, ov, args_list, i, result)) return nullptr;
  }
  return result;
}

} // namespace pycc::rt

// C ABI for argparse
extern "C" void* pycc_argparse_argument_parser() { return ::pycc::rt::argparse_argument_parser(); }
extern "C" void  pycc_argparse_add_argument(void* p, void* n, void* a) { ::pycc::rt::argparse_add_argument(p,n,a); }
extern "C" void* pycc_argparse_parse_args(void* p, void* lst) { return ::pycc::rt::argparse_parse_args(p,lst); }
