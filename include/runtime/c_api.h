// pycc runtime C API for external embedders (C-compatible)
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// GC controls
void pycc_gc_collect(void);
void pycc_gc_set_threshold(size_t bytes);
void pycc_gc_set_background(int enabled);
void pycc_gc_set_conservative(int enabled);
void pycc_gc_write_barrier(void** slot, void* value);

// Boxing
void* pycc_box_int(int64_t v);
void* pycc_box_float(double v);
void* pycc_box_bool(bool v);

// Strings
void* pycc_string_new(const char* data, size_t len);
size_t pycc_string_len(void* s);
void* pycc_string_concat(void* a, void* b);
void* pycc_string_slice(void* s, int64_t start, int64_t len);
void* pycc_string_repeat(void* s, int64_t n);
int pycc_string_contains(void* haystack, void* needle);

// Lists
void* pycc_list_new(uint64_t cap);
void pycc_list_push(void** list_slot, void* elem);
uint64_t pycc_list_len(void* list);
void* pycc_list_get(void* list, int64_t index);
void pycc_list_set(void* list, int64_t index, void* value);

// Objects
void* pycc_object_new(uint64_t fields);
void pycc_object_set(void* obj, uint64_t idx, void* val);
void* pycc_object_get(void* obj, uint64_t idx);

// Dicts
void* pycc_dict_new(uint64_t cap);
void pycc_dict_set(void** dict_slot, void* key, void* value);
void* pycc_dict_get(void* dict, void* key);
uint64_t pycc_dict_len(void* dict);
void* pycc_dict_iter_new(void* dict);
void* pycc_dict_iter_next(void* it);

// Object attribute interop (dictionary-backed per-instance attributes)
void pycc_object_set_attr(void* obj, void* key_string, void* value);
void* pycc_object_get_attr(void* obj, void* key_string);

#ifdef __cplusplus
}
#endif

// Exception and string utilities (C linkage) for codegen
#ifdef __cplusplus
extern "C" {
#endif
void pycc_rt_raise(const char* type_name, const char* message);
int pycc_rt_has_exception(void);
void* pycc_rt_current_exception(void);
void pycc_rt_clear_exception(void);
void* pycc_rt_exception_type(void* exc);
void* pycc_rt_exception_message(void* exc);
int pycc_string_eq(void* a, void* b);
#ifdef __cplusplus
}
#endif
