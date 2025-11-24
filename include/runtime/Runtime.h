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
#include "runtime/TypeTag.h"
#include "runtime/GCStats.h"
#include "runtime/GC.h"

namespace pycc::rt {
    // String objects (opaque)
    void *string_new(const char *data, std::size_t len);

    // String length in bytes
    std::size_t string_len(void *str);

    const char *string_data(void *str);

    void *string_from_cstr(const char *cstr);

    void *string_concat(void *a, void *b);

    // Slice uses Unicode code points (start, length)
    void *string_slice(void *s, std::size_t start, std::size_t len);

    void *string_repeat(void *s, std::size_t n);

    bool string_contains(void *haystack, void *needle);

    // Unicode code point length helper
    std::size_t string_charlen(void *str);

    // Unicode normalization and case handling (optional full support)
    enum class NormalizationForm : uint32_t { NFC = 0, NFD = 1, NFKC = 2, NFKD = 3 };

    // When ICU is available (PYCC_WITH_ICU), these perform full normalization.
    // Otherwise, they return a shallow copy (no-op normalization) for portability.
    void *string_normalize(void *s, NormalizationForm form);

    void *string_casefold(void *s);

    // Encoding/decoding helpers
    // Encode to requested encoding; supported: "utf-8" and "ascii".
    // errors: "strict" (default) or "replace".
    void *string_encode(void *s, const char *encoding, const char *errors);

    // Decode bytes as requested encoding; supported: "utf-8" and "ascii".
    // errors: "strict" (default) or "replace".
    void *bytes_decode(void *b, const char *encoding, const char *errors);

    // Unicode / text encodings (helpers operate on raw buffers)
    bool utf8_is_valid(const char *data, std::size_t len);

    // Bytes (immutable) and ByteArray (mutable) buffers
    void *bytes_new(const void *data, std::size_t len);

    std::size_t bytes_len(void *obj);

    const unsigned char *bytes_data(void *obj);

    void *bytes_slice(void *obj, std::size_t start, std::size_t len);

    void *bytes_concat(void *a, void *b);

    // Find subsequence; returns index or -1 if not found
    int64_t bytes_find(void *haystack, void *needle);

    void *bytearray_new(std::size_t len);

    void *bytearray_from_bytes(void *bytes);

    std::size_t bytearray_len(void *obj);

    int bytearray_get(void *obj, std::size_t index); // returns 0..255 or -1 if OOB
    void bytearray_set(void *obj, std::size_t index, int value);

    void bytearray_append(void *obj, int value);

    // Append bytes content to bytearray up to capacity (no reallocation in this subset)
    void bytearray_extend_from_bytes(void *obj, void *bytes);

    // Boxed primitives (opaque heap objects with value payloads)
    void *box_int(int64_t value);

    int64_t box_int_value(void *obj);

    void *box_float(double value);

    double box_float_value(void *obj);

    void *box_bool(bool value);

    bool box_bool_value(void *obj);

    // List operations (opaque list of ptr values)
    void *list_new(std::size_t capacity);

    void list_push_slot(void **list_slot, void *elem);

    std::size_t list_len(void *list);

    void *list_get(void *list, std::size_t index);

    void list_set(void *list, std::size_t index, void *value);

    // Dict operations (opaque hash map from ptr->ptr; keys typically string objects)
    void *dict_new(std::size_t capacity);

    void dict_set(void **dict_slot, void *key, void *value);

    void *dict_get(void *dict, void *key);

    std::size_t dict_len(void *dict);

    // Dict iteration (iterator object with [0]=dict, [1]=index)
    void *dict_iter_new(void *dict);

    void *dict_iter_next(void *it); // returns next key or nullptr when done

    // Object operations (fixed-size field table of ptr values)
    void *object_new(std::size_t field_count);

    void object_set(void *obj, std::size_t index, void *value);

    void *object_get(void *obj, std::size_t index);

    std::size_t object_field_count(void *obj);

    // Attribute resolution (per-instance attribute dictionary keyed by String objects)
    void object_set_attr(void *obj, void *key_string, void *value);

    void *object_get_attr(void *obj, void *key_string);

    void *object_get_attr_dict(void *obj); // returns the internal dict, may be nullptr

    // Lightweight write barrier hooks for codegen/mutator integration
    void gc_write_barrier(void **slot, void *value);

#define PYCC_GC_WRITE_BARRIER(slot_addr, value_ptr) \
  ::pycc::rt::gc_write_barrier(reinterpret_cast<void**>(slot_addr), reinterpret_cast<void*>(value_ptr))

#define PYCC_GC_ASSIGN(slot_addr, value_expr) \
  do { \
    *(slot_addr) = (value_expr); \
    ::pycc::rt::gc_write_barrier(reinterpret_cast<void**>(slot_addr), reinterpret_cast<void*>(*(slot_addr))); \
  } while (0)

    // Exceptions (thread-local propagation helpers)
    void rt_raise(const char *type_name, const char *message);

    bool rt_has_exception();

    void *rt_current_exception(); // opaque object with two fields: [0]=type(String), [1]=message(String)
    void rt_clear_exception();

    void *rt_exception_type(void *exc);

    void *rt_exception_message(void *exc);

    // Optional chaining/context for richer exceptions
    void rt_exception_set_cause(void *exc, void *cause_exc);
    void *rt_exception_cause(void *exc);
    void rt_exception_set_context(void *exc, void *ctx_exc);
    void *rt_exception_context(void *exc);

    // Basic I/O and OS interop
    void io_write_stdout(void *str);

    void io_write_stderr(void *str);

    void *io_read_file(const char *path); // returns String with file bytes
    bool io_write_file(const char *path, void *str);

    void *os_getenv(const char *name); // returns String or nullptr
    int64_t os_time_ms();

    // Filesystem helpers
    void *os_getcwd(); // returns String
    bool os_mkdir(const char *path, int mode /*octal*/);

    bool os_remove(const char *path);

    bool os_rename(const char *src, const char *dst);

    // Subprocess module shims
    // Execute shell command string. Returns exit code (decoded on POSIX when possible).
    int32_t subprocess_run(void *cmd);

    int32_t subprocess_call(void *cmd);

    int32_t subprocess_check_call(void *cmd); // raises CalledProcessError on non-zero

    // Sys module shims
    void *sys_platform(); // returns String
    void *sys_version(); // returns String
    int64_t sys_maxsize();

    void sys_exit(int32_t code); // test-safe: records last code; may exit in standalone mode

    // JSON module shims
    void *json_dumps(void *obj); // returns String or nullptr on error
    void *json_dumps_ex(void *obj, int indent); // pretty-print with indent spaces (0 = compact)
    void *json_dumps_opts(void *obj, int ensure_ascii, int indent, const char *item_sep, const char *kv_sep,
                          int sort_keys);

    void *json_loads(void *s); // returns parsed object or nullptr on error

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
    void *datetime_now();

    void *datetime_utcnow();

    void *datetime_fromtimestamp(double ts);

    void *datetime_utcfromtimestamp(double ts);

    // pathlib module shims (cross-platform via std::filesystem; paths are UTF-8 strings)
    // Constructors/utilities
    void *pathlib_cwd(); // returns String
    void *pathlib_home(); // returns String (best-effort from HOME/USERPROFILE)
    void *pathlib_join2(void *a, void *b); // join two path segments (String,String)
    void *pathlib_parent(void *p); // dirname
    void *pathlib_basename(void *p); // base name (aka name)
    void *pathlib_suffix(void *p); // extension including leading '.' or empty
    void *pathlib_stem(void *p); // base name without final suffix
    void *pathlib_with_name(void *p, void *name); // replace final path component
    void *pathlib_with_suffix(void *p, void *suffix); // replace final suffix
    void *pathlib_as_posix(void *p); // forward slashes
    void *pathlib_as_uri(void *p); // file:// URI (absolute only)
    void *pathlib_resolve(void *p); // canonical absolute path (best-effort)
    void *pathlib_absolute(void *p); // absolute path without canonicalization
    void *pathlib_parts(void *p); // List of String parts
    bool pathlib_match(void *p, void *pattern); // simple glob ("*" and "?") against name

    // Filesystem effects
    bool pathlib_exists(void *p);

    bool pathlib_is_file(void *p);

    bool pathlib_is_dir(void *p);

    bool pathlib_mkdir(void *p, int mode, int parents, int exist_ok);

    bool pathlib_rmdir(void *p);

    bool pathlib_unlink(void *p);

    bool pathlib_rename(void *src, void *dst);

    // os.path module shims (subset; thin wrappers over pathlib)
    void *os_path_join2(void *a, void *b); // returns String
    void *os_path_dirname(void *p); // returns String
    void *os_path_basename(void *p); // returns String
    void *os_path_splitext(void *p); // returns List [root:str, ext:str]
    void *os_path_abspath(void *p); // returns String
    bool os_path_exists(void *p);

    bool os_path_isfile(void *p);

    bool os_path_isdir(void *p);

    // re module shims (simplified, materialized)
    void *re_compile(void *pattern, int flags);

    void *re_search(void *pattern, void *text, int flags);

    void *re_match(void *pattern, void *text, int flags);

    void *re_fullmatch(void *pattern, void *text, int flags);

    void *re_findall(void *pattern, void *text, int flags);

    void *re_split(void *pattern, void *text, int maxsplit, int flags);

    void *re_sub(void *pattern, void *repl, void *text, int count, int flags);

    void *re_subn(void *pattern, void *repl, void *text, int count, int flags);

    void *re_escape(void *text);

    void *re_finditer(void *pattern, void *text, int flags);

    // fnmatch module shims
    bool fnmatch_fnmatch(void *name, void *pattern);

    bool fnmatch_fnmatchcase(void *name, void *pattern);

    void *fnmatch_filter(void *names_list, void *pattern);

    void *fnmatch_translate(void *pattern);

    // string module shims (subset)
    void *string_capwords(void *s, void *sep_or_null);

    // glob module shims (subset)
    void *glob_glob(void *pattern);

    void *glob_iglob(void *pattern); // materialized as list in this AOT subset
    void *glob_escape(void *pattern);

    // uuid module shims (subset)
    void *uuid_uuid4(); // returns String in canonical form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx

    // base64 module shims (subset)
    void *base64_b64encode(void *data_str_or_bytes); // returns Bytes
    void *base64_b64decode(void *b64_str_or_bytes); // returns Bytes

    // random module shims (subset)
    double random_random();

    int32_t random_randint(int32_t a, int32_t b); // inclusive
    void random_seed(uint64_t seed);

    // secrets module shims (subset)
    void *secrets_token_bytes(int32_t n); // returns Bytes of length n
    void *secrets_token_hex(int32_t n); // returns hex String of length 2*n
    void *secrets_token_urlsafe(int32_t n); // returns urlsafe base64 String (no padding)

    // shutil module shims (subset)
    bool shutil_copyfile(void *src_path, void *dst_path);

    bool shutil_copy(void *src_path, void *dst_path);

    // platform module shims (subset)
    void *platform_system();

    void *platform_machine();

    void *platform_release();

    void *platform_version();

    // errno module shims (subset as functions)
    int32_t errno_EPERM();

    int32_t errno_ENOENT();

    int32_t errno_EEXIST();

    int32_t errno_EISDIR();

    int32_t errno_ENOTDIR();

    int32_t errno_EACCES();

    // heapq module shims (subset)
    void heapq_heappush(void *list, void *value);

    void *heapq_heappop(void *list);

    // bisect module shims (subset)
    int32_t bisect_left(void *sorted_list, void *x);

    int32_t bisect_right(void *sorted_list, void *x);

    // tempfile module shims (subset)
    void *tempfile_gettempdir(); // returns String
    void *tempfile_mkdtemp(); // returns created dir path
    void *tempfile_mkstemp(); // returns List [fd:int, path:str] (fd may be 0)

    // statistics module shims (subset)
    double statistics_mean(void *list_of_numbers);

    double statistics_median(void *list_of_numbers);

    double statistics_stdev(void *list_of_numbers); // sample standard deviation (n-1)
    double statistics_pvariance(void *list_of_numbers); // population variance (n)

    // textwrap module shims (subset)
    void *textwrap_fill(void *s, int32_t width);

    void *textwrap_shorten(void *s, int32_t width);

    void *textwrap_wrap(void *s, int32_t width); // returns List[str]
    void *textwrap_dedent(void *s); // returns Str
    void *textwrap_indent(void *s, void *prefix); // returns Str

    // hashlib module shims (subset, non-cryptographic)
    void *hashlib_sha256(void *data_str_or_bytes); // returns hex String length 64
    void *hashlib_md5(void *data_str_or_bytes); // returns hex String length 32

    // pprint module shims (subset)
    void *pprint_pformat(void *obj);

    // reprlib module shims (subset)
    void *reprlib_repr(void *obj);

    // types module shims (subset)
    void *types_simple_namespace(void *list_of_pairs_opt);

    // colorsys module shims (subset)
    void *colorsys_rgb_to_hsv(double r, double g, double b); // returns List[float,float,float]
    void *colorsys_hsv_to_rgb(double h, double s, double v); // returns List[float,float,float]

    // linecache module shims (subset)
    void *linecache_getline(void *path, int32_t lineno);

    // getpass module shims (subset)
    void *getpass_getuser();

    void *getpass_getpass(void *prompt_opt); // returns empty string in this subset

    // shlex module shims (subset)
    void *shlex_split(void *s);

    void *shlex_join(void *list_of_strings);

    // html module shims (subset)
    void *html_escape(void *s, int32_t quote);

    void *html_unescape(void *s);

    // binascii module shims (subset)
    void *binascii_hexlify(void *data_str_or_bytes); // returns Bytes
    void *binascii_unhexlify(void *hex_str_or_bytes); // returns Bytes

    // hmac module shims (subset)
    void *hmac_digest(void *key_str_or_bytes, void *msg_str_or_bytes, void *digestmod_str); // returns Bytes

    // warnings module shims (subset)
    void warnings_warn(void *msg_str);

    void warnings_simplefilter(void *action_str, void *category_opt);

    // copy module shims (subset)
    void *copy_copy(void *obj);

    void *copy_deepcopy(void *obj);

    // calendar module shims (subset)
    int32_t calendar_isleap(int32_t year);

    void *calendar_monthrange(int32_t year, int32_t month); // returns [weekday(0=Mon), ndays]

    // stat module shims (subset)
    int32_t stat_ifmt(int32_t mode);

    bool stat_isdir(int32_t mode);

    bool stat_isreg(int32_t mode);

    // keyword module shims
    bool keyword_iskeyword(void *s);

    void *keyword_kwlist();

    // collections module shims (materialized helpers)
    void *collections_counter(void *iterable_list);

    void *collections_ordered_dict(void *list_of_pairs);

    void *collections_chainmap(void *list_of_dicts);

    // defaultdict emulation: object with [0]=dict, [1]=default_value
    void *collections_defaultdict_new(void *default_value);

    void *collections_defaultdict_get(void *dd, void *key);

    void collections_defaultdict_set(void *dd, void *key, void *value);

    // array module shims (minimal subset)
    // array.array(typecode: str, initializer: list|None) -> array-obj (opaque object)
    void *array_array(void *typecode_str, void *initializer_list_or_null);

    // Mutating operations
    void array_append(void *arr, void *value);

    void *array_pop(void *arr);

    // Conversion
    void *array_tolist(void *arr);

    // unicodedata module shims (subset)
    void *unicodedata_normalize(void *form_str, void *s_str);

    // struct module shims (subset)
    // struct.pack(fmt: str, values: list) -> Bytes
    void *struct_pack(void *fmt_str, void *values_list);

    // struct.unpack(fmt: str, data: bytes) -> List
    void *struct_unpack(void *fmt_str, void *data_bytes);

    // struct.calcsize(fmt: str) -> int
    int32_t struct_calcsize(void *fmt_str);

    // argparse module shims (subset)
    // Creates a new parser handle
    void *argparse_argument_parser();

    // Add an option: name like "--verbose" or "-v" (aliases may be passed as "-v|--verbose").
    // action: "store_true", "store", or "store_int".
    void argparse_add_argument(void *parser, void *name_str, void *action_str);

    // Parse a list of strings; returns dict of parsed options (bool/int/str values).
    void *argparse_parse_args(void *parser, void *args_list);

    // _abc module shims (minimal accelerator for abc)
    int64_t abc_get_cache_token(); // monotonically increasing token
    bool abc_register(void *abc, void *subclass); // returns true if newly registered
    bool abc_is_registered(void *abc, void *subclass);

    void abc_invalidate_cache(); // increments token
    void abc_reset(); // test helper: clear registry, reset token

    // _aix_support module shims (minimal portable placeholders)
    void *aix_platform(); // returns String (e.g., "aix")
    void *aix_default_libpath(); // returns String (empty in this subset)
    void *aix_ldflags(); // returns List of Strings (empty)

    // _android_support module shims (minimal portable placeholders)
    void *android_platform(); // returns String ("android")
    void *android_default_libdir(); // returns String (empty)
    void *android_ldflags(); // returns List of Strings (empty)

    // _apple_support module shims (minimal portable placeholders)
    void *apple_platform(); // returns String ("darwin")
    void *apple_default_sdkroot(); // returns String (empty)
    void *apple_ldflags(); // returns List of Strings (empty)

    // _ast module shims (minimal helpers for compatibility)
    void *ast_dump(void *obj); // returns String (placeholder dump)
    void *ast_iter_fields(void *obj); // returns empty List
    void *ast_walk(void *obj); // returns empty List
    void *ast_copy_location(void *new_node, void *old_node); // returns new_node
    void *ast_fix_missing_locations(void *node); // returns node
    void *ast_get_docstring(void *node); // returns empty String

    // _asyncio module shims (minimal helpers)
    void *asyncio_get_event_loop(); // returns opaque object
    void *asyncio_future_new(); // returns opaque object
    void asyncio_future_set_result(void *fut, void *result);

    void *asyncio_future_result(void *fut); // returns stored result (ptr or nullptr)
    bool asyncio_future_done(void *fut); // true if result set
    void asyncio_sleep(double seconds); // delegates to time_sleep

    // Itertools (materialized list-based helpers for AOT subset)
    // chain: concatenates two or more lists; from_iterable flattens a list of lists.
    void *itertools_chain2(void *a, void *b);

    void *itertools_chain_from_iterable(void *list_of_lists);

    // product of two lists -> list of 2-element lists
    void *itertools_product2(void *a, void *b);

    // permutations/combinations over a list; results as list of lists
    void *itertools_permutations(void *a, int r /* <=0 means len(a) */);

    void *itertools_combinations(void *a, int r);

    void *itertools_combinations_with_replacement(void *a, int r);

    // zip_longest of two lists with fillvalue
    void *itertools_zip_longest2(void *a, void *b, void *fillvalue);

    // islice(list, start, stop, step)
    void *itertools_islice(void *a, int start, int stop, int step);

    // accumulate numbers (int/float) with sum; returns list of prefix sums
    void *itertools_accumulate_sum(void *a);

    // repeat(obj, times)
    void *itertools_repeat(void *obj, int times);

    // pairwise(list) -> list of [a,b]
    void *itertools_pairwise(void *a);

    // batched(list, n) -> list of batches (lists)
    void *itertools_batched(void *a, int n);

    // compress(data, selectors) -> elements where selector truthy
    void *itertools_compress(void *data, void *selectors);

    // operator module shims (numeric + boolean subset)
    void *operator_add(void *a, void *b); // returns boxed int/float
    void *operator_sub(void *a, void *b); // returns boxed int/float
    void *operator_mul(void *a, void *b); // returns boxed int/float
    void *operator_truediv(void *a, void *b); // returns boxed float
    void *operator_neg(void *a); // returns boxed int/float
    bool operator_eq(void *a, void *b);

    bool operator_lt(void *a, void *b);

    bool operator_not_(void *a);

    bool operator_truth(void *a);

    // Concurrency scaffolding
    using RtStart = void(*)(const void *payload, std::size_t len, void **ret, std::size_t *ret_len);
    struct RtThreadHandle; // opaque
    struct RtChannelHandle; // opaque
    struct RtAtomicIntHandle; // opaque

    // Threads
    RtThreadHandle *rt_spawn(RtStart fn, const void *payload, std::size_t len);

    bool rt_join(RtThreadHandle *h, void **ret, std::size_t *ret_len);

    void rt_thread_handle_destroy(RtThreadHandle *h);

    // Channels (ptr payload, blocking semantics)
    RtChannelHandle *chan_new(std::size_t capacity);

    void chan_close(RtChannelHandle *ch);

    void chan_send(RtChannelHandle *ch, void *value);

    void *chan_recv(RtChannelHandle *ch);

    // Atomics (64-bit)
    RtAtomicIntHandle *atomic_int_new(long long initial);

    long long atomic_int_load(RtAtomicIntHandle *a);

    void atomic_int_store(RtAtomicIntHandle *a, long long v);

    long long atomic_int_add_fetch(RtAtomicIntHandle *a, long long delta);
} // namespace pycc::rt
