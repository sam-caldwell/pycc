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
  Object = 6,
  Dict = 7
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
const char* string_data(void* str);
void* string_from_cstr(const char* cstr);
void* string_concat(void* a, void* b);
void* string_slice(void* s, std::size_t start, std::size_t len);
void* string_repeat(void* s, std::size_t n);
bool string_contains(void* haystack, void* needle);

// Unicode / text encodings (helpers operate on raw buffers)
bool utf8_is_valid(const char* data, std::size_t len);

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
void* list_get(void* list, std::size_t index);
void list_set(void* list, std::size_t index, void* value);

// Dict operations (opaque hash map from ptr->ptr; keys typically string objects)
void* dict_new(std::size_t capacity);
void dict_set(void** dict_slot, void* key, void* value);
void* dict_get(void* dict, void* key);
std::size_t dict_len(void* dict);
// Dict iteration (iterator object with [0]=dict, [1]=index)
void* dict_iter_new(void* dict);
void* dict_iter_next(void* it); // returns next key or nullptr when done

// Object operations (fixed-size field table of ptr values)
void* object_new(std::size_t field_count);
void object_set(void* obj, std::size_t index, void* value);
void* object_get(void* obj, std::size_t index);
std::size_t object_field_count(void* obj);

// Attribute resolution (per-instance attribute dictionary keyed by String objects)
void object_set_attr(void* obj, void* key_string, void* value);
void* object_get_attr(void* obj, void* key_string);
void* object_get_attr_dict(void* obj); // returns the internal dict, may be nullptr

// Lightweight write barrier hooks for codegen/mutator integration
void gc_write_barrier(void** slot, void* value);

#define PYCC_GC_WRITE_BARRIER(slot_addr, value_ptr) \
  ::pycc::rt::gc_write_barrier(reinterpret_cast<void**>(slot_addr), reinterpret_cast<void*>(value_ptr))

#define PYCC_GC_ASSIGN(slot_addr, value_expr) \
  do { \
    *(slot_addr) = (value_expr); \
    ::pycc::rt::gc_write_barrier(reinterpret_cast<void**>(slot_addr), reinterpret_cast<void*>(*(slot_addr))); \
  } while (0)

// Exceptions (thread-local propagation helpers)
void rt_raise(const char* type_name, const char* message);
bool rt_has_exception();
void* rt_current_exception(); // opaque object with two fields: [0]=type(String), [1]=message(String)
void rt_clear_exception();
void* rt_exception_type(void* exc);
void* rt_exception_message(void* exc);

// Basic I/O and OS interop
void io_write_stdout(void* str);
void io_write_stderr(void* str);
void* io_read_file(const char* path); // returns String with file bytes
bool io_write_file(const char* path, void* str);
void* os_getenv(const char* name); // returns String or nullptr
int64_t os_time_ms();

} // namespace pycc::rt
