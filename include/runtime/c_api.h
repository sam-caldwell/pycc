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

// Lists
void* pycc_list_new(uint64_t cap);
void pycc_list_push(void** list_slot, void* elem);
uint64_t pycc_list_len(void* list);

// Objects
void* pycc_object_new(uint64_t fields);
void pycc_object_set(void* obj, uint64_t idx, void* val);
void* pycc_object_get(void* obj, uint64_t idx);

#ifdef __cplusplus
}
#endif

