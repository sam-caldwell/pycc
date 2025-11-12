/***
 * Name: pycc::rt (Runtime API)
 * Purpose: Minimal runtime and GC v1 interface for pycc.
 * Theory of Operation:
 *   - Provides a simple precise mark-sweep collector with an explicit root set.
 *   - Exposes string allocation helpers suitable for interop with generated code/tests.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace pycc::rt {

enum class TypeTag : uint32_t { String = 1 };

struct RuntimeStats {
  uint64_t numAllocated{0};
  uint64_t numFreed{0};
  uint64_t numCollections{0};
  uint64_t bytesAllocated{0};
  uint64_t bytesLive{0};
};

// GC controls
void gc_set_threshold(std::size_t bytes);
void gc_collect();
void gc_register_root(void** addr);
void gc_unregister_root(void** addr);
RuntimeStats gc_stats();
void gc_reset_for_tests();

// String objects (opaque)
void* string_new(const char* data, std::size_t len);
std::size_t string_len(void* str);

} // namespace pycc::rt

