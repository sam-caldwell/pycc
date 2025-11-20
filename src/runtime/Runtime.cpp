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
#include <mutex>
#include <optional>
#include <pthread.h>
#include <thread>
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
  ObjectHeader* next{nullptr};
};

struct StringPayload { std::size_t len{}; /* char data[] follows */ };

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
  auto* mem = static_cast<unsigned char*>(::operator new(total));
  auto* header = reinterpret_cast<ObjectHeader*>(mem); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  header->mark = 0;
  header->tag = static_cast<uint32_t>(tag);
  header->size = total;
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
  ::operator delete(header);
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
      cur->mark = 0; // clear for next cycle
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
extern "C" void* pycc_string_concat(void* a, void* b) { return string_concat(a, b); }
extern "C" void* pycc_string_slice(void* s, int64_t start, int64_t len) {
  std::size_t L = string_len(s);
  int64_t st = start; if (st < 0) st += static_cast<int64_t>(L); if (st < 0) st = 0;
  int64_t ln = len; if (ln < 0) ln = 0;
  return string_slice(s, static_cast<std::size_t>(st), static_cast<std::size_t>(ln));
}
extern "C" void* pycc_string_concat(void* a, void* b) { return string_concat(a, b); }
extern "C" void* pycc_string_repeat(void* s, int64_t n) {
  if (n <= 0) return string_new("", 0);
  const char* d = string_data(s);
  std::size_t L = string_len(s);
  std::string tmp; tmp.resize(L * static_cast<std::size_t>(n));
  for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) { std::memcpy(tmp.data() + (i * L), d, L); }
  return string_new(tmp.data(), tmp.size());
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
  const std::size_t L = string_len(s);
  if (start > L) start = L;
  std::size_t n = (start + len > L) ? (L - start) : len;
  return string_new(d + start, n);
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

} // namespace pycc::rt
