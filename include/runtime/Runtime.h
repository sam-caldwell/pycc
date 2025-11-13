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

enum class TypeTag : uint32_t {
  String = 1,
  Int = 2,
  Float = 3,
  Bool = 4,
  List = 5,
  Object = 6
};

struct RuntimeStats {
  uint64_t numAllocated{0};
  uint64_t numFreed{0};
  uint64_t numCollections{0};
  uint64_t bytesAllocated{0};
  uint64_t bytesLive{0};
  uint64_t peakBytesLive{0};
  uint64_t lastReclaimedBytes{0};
};

struct GcTelemetry {
  double allocRateBytesPerSec{0.0};  // recent bytes/sec
  double pressure{0.0};              // bytesLive / threshold (0..inf)
};

// GC controls
void gc_set_threshold(std::size_t bytes);
void gc_set_conservative(bool enabled);
void gc_set_background(bool enabled);
void gc_collect();
void gc_register_root(void** addr);
void gc_unregister_root(void** addr);
RuntimeStats gc_stats();
GcTelemetry gc_telemetry();

// Barrier mode (0 = incremental-update, 1 = SATB)
void gc_set_barrier_mode(int mode);
void gc_pre_barrier(void** slot);
void gc_reset_for_tests();

// String objects (opaque)
void* string_new(const char* data, std::size_t len);
std::size_t string_len(void* str);

// Boxed primitives (opaque heap objects with value payloads)
void* box_int(int64_t value);
int64_t box_int_value(void* obj);

void* box_float(double value);
double box_float_value(void* obj);

void* box_bool(bool value);
bool box_bool_value(void* obj);

// List operations (opaque list of ptr values)
void* list_new(std::size_t capacity);
void list_push_slot(void** list_slot, void* elem);
std::size_t list_len(void* list);

// Object operations (fixed-size field table of ptr values)
void* object_new(std::size_t field_count);
void object_set(void* obj, std::size_t index, void* value);
void* object_get(void* obj, std::size_t index);
std::size_t object_field_count(void* obj);

// Lightweight write barrier hooks for codegen/mutator integration
void gc_write_barrier(void** slot, void* value);

#define PYCC_GC_WRITE_BARRIER(slot_addr, value_ptr) \
  ::pycc::rt::gc_write_barrier(reinterpret_cast<void**>(slot_addr), reinterpret_cast<void*>(value_ptr))

#define PYCC_GC_ASSIGN(slot_addr, value_expr) \
  do { \
    *(slot_addr) = (value_expr); \
    ::pycc::rt::gc_write_barrier(reinterpret_cast<void**>(slot_addr), reinterpret_cast<void*>(*(slot_addr))); \
  } while (0)

} // namespace pycc::rt
