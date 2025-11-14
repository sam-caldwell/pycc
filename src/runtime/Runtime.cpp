/***
 * Name: pycc::rt (Runtime impl)
 * Purpose: Minimal precise mark-sweep GC and string objects.
 */
#include "runtime/Runtime.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>
#include <pthread.h>
#include <cstddef>
#include <cinttypes>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>

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

// Forward decl for adaptive controller
static void adapt_controller();

// Request background GC if pressure is above the threshold.
// Requires caller to hold g_mu; avoids synchronous collection during allocation
// to prevent collecting newly allocated yet-unrooted objects (UAF risk).
static inline void maybe_request_bg_gc_unlocked() {
  if (g_stats.bytesLive <= g_threshold) { return; }
  if (!g_bg_enabled.load(std::memory_order_relaxed)) { return; }
  g_bg_requested.store(true, std::memory_order_relaxed);
  std::lock_guard<std::mutex> nlk(g_bg_mu);
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
  ::operator delete(header);
}

// Forward declaration for interior marking
static ObjectHeader* find_object_for_pointer(const void* ptr);

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
      auto** items = reinterpret_cast<void**>(payload + 2);
      for (std::size_t i = 0; i < len; ++i) {
        void* valuePtr = items[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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
      auto** values = reinterpret_cast<void**>(payload + 1);
      for (std::size_t i = 0; i < fields; ++i) {
        void* valuePtr = values[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (valuePtr == nullptr) { continue; }
        if (ObjectHeader* headerPtr = find_object_for_pointer(valuePtr)) { mark(headerPtr); }
      }
      break;
    }
  }
}

static void mark_from_roots() {
  for (void** slot : g_roots) {
    void* p = *slot;
    if (p == nullptr) { continue; }
    auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(p) - sizeof(ObjectHeader)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    mark(h);
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
    std::lock_guard<std::mutex> lockGuard(g_rem_mu);
    tmp.swap(g_remembered);
  }
  for (void* valuePtr : tmp) {
    if (valuePtr == nullptr) { continue; }
    if (ObjectHeader* header = find_object_for_pointer(valuePtr)) { mark(header); }
  }
}

static bool get_stack_bounds(void*& low, void*& high) {
  low = nullptr; high = nullptr;
  // Current thread only
#ifdef __APPLE__
  pthread_t self = pthread_self();
  void* stackaddr = pthread_get_stackaddr_np(self);
  size_t stacksize = pthread_get_stacksize_np(self);
  if (stackaddr != nullptr && stacksize > 0U) {
    // On macOS, stackaddr is the HIGH address, stack grows down
    high = stackaddr;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    low = static_cast<unsigned char*>(stackaddr) - stacksize;
    return true;
  }
  return false;
#else
#ifdef __linux__
  pthread_attr_t attr;
  if (pthread_getattr_np(pthread_self(), &attr) != 0) { return false; }
  void* stackaddr = nullptr; size_t stacksize = 0;
  if (pthread_attr_getstack(&attr, &stackaddr, &stacksize) != 0) { pthread_attr_destroy(&attr); return false; }
  pthread_attr_destroy(&attr);
  low = stackaddr;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  high = static_cast<unsigned char*>(stackaddr) + stacksize;
  return true;
#else
  // Fallback: approximate using address of local variable as low bound and assume 8MB stack
  unsigned char local{};
  low = &local;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  high = static_cast<unsigned char*>(low) + (8U << 20U);
  return true;
#endif
#endif
}

// NOLINTNEXTLINE(readability-function-size)
static void mark_from_stack() {
  void* low = nullptr; void* high = nullptr;
  if (!get_stack_bounds(low, high)) { return; }
  // Align low up to word alignment
  std::uintptr_t lowAddr = reinterpret_cast<std::uintptr_t>(low); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::size_t align = alignof(std::uintptr_t);
  lowAddr = (lowAddr + (align - 1U)) & ~(static_cast<std::uintptr_t>(align) - 1U);
  auto* scanPtr = reinterpret_cast<std::uintptr_t*>(lowAddr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  auto* end = reinterpret_cast<std::uintptr_t*>(high);        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  // Iterate word-sized chunks across the stack region
  while (scanPtr < end) {
    void* candidate = nullptr; candidate = reinterpret_cast<void*>(*scanPtr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (candidate != nullptr) {
      if (ObjectHeader* header = find_object_for_pointer(candidate)) { mark(header); }
    }
    ++scanPtr; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
}

// NOLINTNEXTLINE(readability-function-size)
static bool mark_from_stack_slice(std::size_t words_budget) {
  if (g_stack_scan_cur == nullptr || g_stack_scan_end == nullptr) {
    void* low = nullptr; void* high = nullptr;
    if (!get_stack_bounds(low, high)) { return true; }
    std::uintptr_t lowAddr = reinterpret_cast<std::uintptr_t>(low); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::size_t align = alignof(std::uintptr_t);
    lowAddr = (lowAddr + (align - 1U)) & ~(static_cast<std::uintptr_t>(align) - 1U);
    g_stack_scan_cur = reinterpret_cast<std::uintptr_t*>(lowAddr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    g_stack_scan_end = reinterpret_cast<std::uintptr_t*>(high);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  }
  std::size_t wordsScanned = 0;
  while (g_stack_scan_cur < g_stack_scan_end && wordsScanned < words_budget) {
    void* candidate = reinterpret_cast<void*>(*g_stack_scan_cur); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_bg_enabled.load(std::memory_order_relaxed)) {
    g_bg_requested.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock2(g_bg_mu);
    g_bg_cv.notify_one();
    return; // quick return; background will service GC
  }
  g_stats.numCollections++;
  mark_from_roots();
  if (g_conservative) { mark_from_stack(); }
  mark_from_remembered_locked();
  sweep();
}

void gc_set_threshold(std::size_t bytes) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_threshold = bytes;
}

void gc_set_conservative(bool enabled) {
  std::lock_guard<std::mutex> lock(g_mu);
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
        std::lock_guard<std::mutex> lock2(g_mu);
        g_stats.numCollections++;
        mark_from_roots();
        mark_from_remembered_locked();
      }
      if (g_conservative) {
        auto tStart = std::chrono::steady_clock::now();
        while (true) {
          bool done = mark_from_stack_slice(kStackSliceWords);
          if (done) { break; }
          if (std::chrono::steady_clock::now() - tStart > slice_budget) { std::this_thread::yield(); tStart = std::chrono::steady_clock::now(); }
        }
        // Reset scan pointers
        g_stack_scan_cur = nullptr; g_stack_scan_end = nullptr;
        // Finalize with any remembered writes while marking
        std::lock_guard<std::mutex> lock2(g_mu);
        mark_from_remembered_locked();
      }
      // Sweeping in slices while holding g_mu briefly
      const std::chrono::nanoseconds min_hold(kMinLockHoldNs); // small lock hold
      for (;;) {
        auto t_lock_start = std::chrono::steady_clock::now();
        std::size_t reclaimed = 0;
        {
          std::lock_guard<std::mutex> lock3(g_mu);
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
    }
  });
  g_bg_thread.detach();
}

void gc_set_background(bool enabled) {
  std::lock_guard<std::mutex> lock(g_mu);
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
  std::lock_guard<std::mutex> lockGuard(g_rem_mu);
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
  void* old = *slot; // NOLINT(clang-analyzer-core.uninitialized.Assign)
  if (old == nullptr) { return; }
  std::lock_guard<std::mutex> lockGuard(g_rem_mu);
  g_remembered.push_back(old);
}

void gc_set_barrier_mode(int mode) {
  g_barrier_mode.store((mode != 0) ? 1 : 0, std::memory_order_relaxed);
}

// C ABI wrappers for list/object to simplify IR calls later
extern "C" void* pycc_list_new(uint64_t cap) { return list_new(static_cast<std::size_t>(cap)); }
extern "C" void pycc_list_push(void** list_slot, void* elem) { list_push_slot(list_slot, elem); }
extern "C" uint64_t pycc_list_len(void* list) { return static_cast<uint64_t>(list_len(list)); }
extern "C" void* pycc_object_new(uint64_t fields) { return object_new(static_cast<std::size_t>(fields)); }
extern "C" void pycc_object_set(void* obj, uint64_t idx, void* val) { object_set(obj, static_cast<std::size_t>(idx), val); }
extern "C" void* pycc_object_get(void* obj, uint64_t idx) { return object_get(obj, static_cast<std::size_t>(idx)); }
extern "C" void* pycc_box_int(int64_t value) { return box_int(value); }
extern "C" void* pycc_box_float(double value) { return box_float(value); }
extern "C" void* pycc_box_bool(bool value) { return box_bool(value); }
extern "C" void* pycc_string_new(const char* data, size_t length) { return string_new(data, length); }
extern "C" uint64_t pycc_string_len(void* str) { return static_cast<uint64_t>(string_len(str)); }

void gc_register_root(void** addr) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_roots.push_back(addr);
}

void gc_unregister_root(void** addr) {
  std::lock_guard<std::mutex> lock(g_mu);
  auto iter = std::find(g_roots.begin(), g_roots.end(), addr);
  if (iter != g_roots.end()) { g_roots.erase(iter); }
}

RuntimeStats gc_stats() {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_stats;
}

void gc_reset_for_tests() {
  std::lock_guard<std::mutex> lock(g_mu);
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
  std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = sizeof(StringPayload) + len + 1; // include NUL
  auto* payloadBytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::String));
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

// Boxed primitives
void* box_int(int64_t value) {
  std::lock_guard<std::mutex> lock(g_mu);
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
  std::lock_guard<std::mutex> lock(g_mu);
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
  std::lock_guard<std::mutex> lock(g_mu);
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
void* list_new(std::size_t capacity) {
  std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = (sizeof(std::size_t) * 2) + (capacity * sizeof(void*)); // len, cap, items[]
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::List));
  auto* meta = reinterpret_cast<std::size_t*>(bytes); // NOLINT
  meta[0] = 0; meta[1] = capacity; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto** items = reinterpret_cast<void**>(meta + 2); // NOLINT
  for (std::size_t i = 0; i < capacity; ++i) { items[i] = nullptr; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  maybe_request_bg_gc_unlocked();
  return bytes;
}

void list_push_slot(void** list_slot, void* elem) {
  if (list_slot == nullptr) { return; }
  std::lock_guard<std::mutex> lock(g_mu);
  auto* list = *list_slot;
  if (list == nullptr) {
    list = list_new(kDefaultListCapacity);
    // update slot with barrier
    gc_pre_barrier(list_slot);
    gc_write_barrier(list_slot, list);
    *list_slot = list;
  }
  auto* meta = reinterpret_cast<std::size_t*>(list); // NOLINT
  std::size_t len = meta[0];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::size_t cap = meta[1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto** items = reinterpret_cast<void**>(meta + 2); // NOLINT
  if (len >= cap) {
    std::size_t newCap = (cap == 0U) ? kDefaultListCapacity : (cap * 2U);
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
  auto* meta = reinterpret_cast<std::size_t*>(list); // NOLINT
  return meta[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

// Objects (fixed-size field table)
void* object_new(std::size_t field_count) {
  std::lock_guard<std::mutex> lock(g_mu);
  const std::size_t payloadSize = sizeof(std::size_t) + (field_count * sizeof(void*)); // fields, values[]
  auto* bytes = static_cast<unsigned char*>(alloc_raw(payloadSize, TypeTag::Object));
  auto* meta = reinterpret_cast<std::size_t*>(bytes); // NOLINT
  meta[0] = field_count; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto** vals = reinterpret_cast<void**>(meta + 1); // NOLINT
  for (std::size_t i = 0; i < field_count; ++i) { vals[i] = nullptr; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  maybe_request_bg_gc_unlocked();
  return bytes;
}

void object_set(void* obj, std::size_t index, void* value) {
  if (obj == nullptr) { return; }
  std::lock_guard<std::mutex> lock(g_mu);
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
  auto** vals = reinterpret_cast<void**>(meta + 1); // NOLINT
  return vals[index]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

std::size_t object_field_count(void* obj) {
  if (obj == nullptr) { return 0; }
  auto* meta = reinterpret_cast<std::size_t*>(obj); // NOLINT
  return meta[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}
GcTelemetry gc_telemetry() {
  uint64_t live_now = 0; std::size_t thr = 0;
  {
    std::lock_guard<std::mutex> lock(g_mu);
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
    std::lock_guard<std::mutex> lock(g_mu);
    alloc_now = g_stats.bytesAllocated;
  }
  const uint64_t delta_alloc = (alloc_now > last_alloc) ? (alloc_now - last_alloc) : 0U;
  g_last_bytes_alloc.store(alloc_now, std::memory_order_relaxed);

  const double alloc_rate = static_cast<double>(delta_alloc) / static_cast<double>(delta_ms + 1U); // bytes/ms
  double pressure = 0.0;
  {
    std::lock_guard<std::mutex> lock(g_mu);
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

} // namespace pycc::rt
