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
  , Bytes = 8
  , ByteArray = 9
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
// String length in bytes
std::size_t string_len(void* str);
const char* string_data(void* str);
void* string_from_cstr(const char* cstr);
void* string_concat(void* a, void* b);
// Slice uses Unicode code points (start, length)
void* string_slice(void* s, std::size_t start, std::size_t len);
void* string_repeat(void* s, std::size_t n);
bool string_contains(void* haystack, void* needle);
// Unicode code point length helper
std::size_t string_charlen(void* str);

// Unicode normalization and case handling (optional full support)
enum class NormalizationForm : uint32_t { NFC = 0, NFD = 1, NFKC = 2, NFKD = 3 };
// When ICU is available (PYCC_WITH_ICU), these perform full normalization.
// Otherwise they return a shallow copy (no-op normalization) for portability.
void* string_normalize(void* s, NormalizationForm form);
void* string_casefold(void* s);

// Encoding/decoding helpers
// Encode to requested encoding; supported: "utf-8" and "ascii".
// errors: "strict" (default) or "replace".
void* string_encode(void* s, const char* encoding, const char* errors);
// Decode bytes as requested encoding; supported: "utf-8" and "ascii".
// errors: "strict" (default) or "replace".
void* bytes_decode(void* b, const char* encoding, const char* errors);

// Unicode / text encodings (helpers operate on raw buffers)
bool utf8_is_valid(const char* data, std::size_t len);

// Bytes (immutable) and ByteArray (mutable) buffers
void* bytes_new(const void* data, std::size_t len);
std::size_t bytes_len(void* obj);
const unsigned char* bytes_data(void* obj);
void* bytes_slice(void* obj, std::size_t start, std::size_t len);
void* bytes_concat(void* a, void* b);

void* bytearray_new(std::size_t len);
void* bytearray_from_bytes(void* bytes);
std::size_t bytearray_len(void* obj);
int bytearray_get(void* obj, std::size_t index); // returns 0..255 or -1 if OOB
void bytearray_set(void* obj, std::size_t index, int value);
void bytearray_append(void* obj, int value);

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
// Filesystem helpers
void* os_getcwd();        // returns String
bool os_mkdir(const char* path, int mode /*octal*/);
bool os_remove(const char* path);
bool os_rename(const char* src, const char* dst);

// Subprocess module shims
// Execute shell command string. Returns exit code (decoded on POSIX when possible).
int32_t subprocess_run(void* cmd);
int32_t subprocess_call(void* cmd);
int32_t subprocess_check_call(void* cmd); // raises CalledProcessError on non-zero

// Sys module shims
void* sys_platform(); // returns String
void* sys_version();  // returns String
int64_t sys_maxsize();
void sys_exit(int32_t code); // test-safe: records last code; may exit in standalone mode

// JSON module shims
void* json_dumps(void* obj); // returns String or nullptr on error
void* json_dumps_ex(void* obj, int indent); // pretty-print with indent spaces (0 = compact)
void* json_dumps_opts(void* obj, int ensure_ascii, int indent, const char* item_sep, const char* kv_sep, int sort_keys);
void* json_loads(void* s);   // returns parsed object or nullptr on error

// Time module shims
double time_time();
int64_t time_time_ns();
double time_monotonic();
int64_t time_monotonic_ns();
double time_perf_counter();
int64_t time_perf_counter_ns();
double time_process_time();
void time_sleep(double seconds);

// Datetime module shims (return ISO-8601 strings)
void* datetime_now();
void* datetime_utcnow();
void* datetime_fromtimestamp(double ts);
void* datetime_utcfromtimestamp(double ts);

// pathlib module shims (cross-platform via std::filesystem; paths are UTF-8 strings)
// Constructors/utilities
void* pathlib_cwd();                 // returns String
void* pathlib_home();                // returns String (best-effort from HOME/USERPROFILE)
void* pathlib_join2(void* a, void* b);           // join two path segments (String,String)
void* pathlib_parent(void* p);                    // dirname
void* pathlib_basename(void* p);                  // base name (aka name)
void* pathlib_suffix(void* p);                    // extension including leading '.' or empty
void* pathlib_stem(void* p);                      // base name without final suffix
void* pathlib_with_name(void* p, void* name);     // replace final path component
void* pathlib_with_suffix(void* p, void* suffix); // replace final suffix
void* pathlib_as_posix(void* p);                  // forward slashes
void* pathlib_as_uri(void* p);                    // file:// URI (absolute only)
void* pathlib_resolve(void* p);                   // canonical absolute path (best-effort)
void* pathlib_absolute(void* p);                  // absolute path without canonicalization
void* pathlib_parts(void* p);                     // List of String parts
bool   pathlib_match(void* p, void* pattern);     // simple glob ("*" and "?") against name

// Filesystem effects
bool pathlib_exists(void* p);
bool pathlib_is_file(void* p);
bool pathlib_is_dir(void* p);
bool pathlib_mkdir(void* p, int mode, int parents, int exist_ok);
bool pathlib_rmdir(void* p);
bool pathlib_unlink(void* p);
bool pathlib_rename(void* src, void* dst);

// re module shims (simplified, materialized)
void* re_compile(void* pattern, int flags);
void* re_search(void* pattern, void* text, int flags);
void* re_match(void* pattern, void* text, int flags);
void* re_fullmatch(void* pattern, void* text, int flags);
void* re_findall(void* pattern, void* text, int flags);
void* re_split(void* pattern, void* text, int maxsplit, int flags);
void* re_sub(void* pattern, void* repl, void* text, int count, int flags);
void* re_subn(void* pattern, void* repl, void* text, int count, int flags);
void* re_escape(void* text);
void* re_finditer(void* pattern, void* text, int flags);

// collections module shims (materialized helpers)
void* collections_counter(void* iterable_list);
void* collections_ordered_dict(void* list_of_pairs);
void* collections_chainmap(void* list_of_dicts);
// defaultdict emulation: object with [0]=dict, [1]=default_value
void* collections_defaultdict_new(void* default_value);
void* collections_defaultdict_get(void* dd, void* key);
void  collections_defaultdict_set(void* dd, void* key, void* value);

// Itertools (materialized list-based helpers for AOT subset)
// chain: concatenates two or more lists; from_iterable flattens a list of lists.
void* itertools_chain2(void* a, void* b);
void* itertools_chain_from_iterable(void* list_of_lists);
// product of two lists -> list of 2-element lists
void* itertools_product2(void* a, void* b);
// permutations/combinations over a list; results as list of lists
void* itertools_permutations(void* a, int r /* <=0 means len(a) */);
void* itertools_combinations(void* a, int r);
void* itertools_combinations_with_replacement(void* a, int r);
// zip_longest of two lists with fillvalue
void* itertools_zip_longest2(void* a, void* b, void* fillvalue);
// islice(list, start, stop, step)
void* itertools_islice(void* a, int start, int stop, int step);
// accumulate numbers (int/float) with sum; returns list of prefix sums
void* itertools_accumulate_sum(void* a);
// repeat(obj, times)
void* itertools_repeat(void* obj, int times);
// pairwise(list) -> list of [a,b]
void* itertools_pairwise(void* a);
// batched(list, n) -> list of batches (lists)
void* itertools_batched(void* a, int n);
// compress(data, selectors) -> elements where selector truthy
void* itertools_compress(void* data, void* selectors);

// Concurrency scaffolding
using RtStart = void(*)(const void* payload, std::size_t len, void** ret, std::size_t* ret_len);
struct RtThreadHandle; // opaque
struct RtChannelHandle; // opaque
struct RtAtomicIntHandle; // opaque

// Threads
RtThreadHandle* rt_spawn(RtStart fn, const void* payload, std::size_t len);
bool rt_join(RtThreadHandle* h, void** ret, std::size_t* ret_len);
void rt_thread_handle_destroy(RtThreadHandle* h);

// Channels (ptr payload, blocking semantics)
RtChannelHandle* chan_new(std::size_t capacity);
void chan_close(RtChannelHandle* ch);
void chan_send(RtChannelHandle* ch, void* value);
void* chan_recv(RtChannelHandle* ch);

// Atomics (64-bit)
RtAtomicIntHandle* atomic_int_new(long long initial);
long long atomic_int_load(RtAtomicIntHandle* a);
void atomic_int_store(RtAtomicIntHandle* a, long long v);
long long atomic_int_add_fetch(RtAtomicIntHandle* a, long long delta);
} // namespace pycc::rt
