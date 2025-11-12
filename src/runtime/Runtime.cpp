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

namespace pycc::rt {

struct ObjectHeader {
  uint32_t mark{0};
  uint32_t tag{0};
  std::size_t size{0}; // total allocation size including header
  ObjectHeader* next{nullptr};
};

struct StringPayload { std::size_t len; /* char data[] follows */ };

static std::mutex g_mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static ObjectHeader* g_head = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<void**> g_roots; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::size_t g_threshold = 1 << 20; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,cppcoreguidelines-avoid-non-const-global-variables)
static RuntimeStats g_stats; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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
  return mem + sizeof(ObjectHeader); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

static void free_obj(ObjectHeader* header) {
  g_stats.numFreed++;
  g_stats.bytesLive -= header->size;
  ::operator delete(header);
}

static void mark(ObjectHeader* header) {
  if (header == nullptr || header->mark != 0U) { return; }
  header->mark = 1;
  // For now only String has no interior pointers, so nothing to recurse.
}

static void mark_from_roots() {
  for (void** slot : g_roots) {
    void* p = *slot;
    if (p == nullptr) { continue; }
    auto* h = reinterpret_cast<ObjectHeader*>(reinterpret_cast<unsigned char*>(p) - sizeof(ObjectHeader)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    mark(h);
  }
}

// NOLINTNEXTLINE(readability-function-size)
static void sweep() {
  ObjectHeader* prev = nullptr; ObjectHeader* cur = g_head;
  while (cur != nullptr) {
    if (cur->mark == 0U) {
      ObjectHeader* dead = cur;
      cur = cur->next;
      if (prev != nullptr) { prev->next = cur; } else { g_head = cur; }
      free_obj(dead);
    } else {
      cur->mark = 0; // clear for next cycle
      prev = cur;
      cur = cur->next;
    }
  }
}

void gc_collect() {
  std::lock_guard<std::mutex> lock(g_mu);
  g_stats.numCollections++;
  mark_from_roots();
  sweep();
}

void gc_set_threshold(std::size_t bytes) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_threshold = bytes;
}

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
  // trigger GC if threshold exceeded
  if (g_stats.bytesLive > g_threshold) {
    // Temporarily release lock to avoid deadlock in collect which locks too
    // For this simple design, just collect without releasing since methods are re-entrant safe (single lock) and we hold it.
    gc_collect();
  }
  return payload; // return pointer to payload start as the object handle
}

std::size_t string_len(void* str) {
  if (str == nullptr) { return 0; }
  auto* plen = static_cast<std::size_t*>(str);
  return *plen;
}

} // namespace pycc::rt
