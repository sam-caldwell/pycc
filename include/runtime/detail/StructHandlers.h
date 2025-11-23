/**
 * @file
 * @brief Internal struct pack/unpack helpers with thin public wrappers.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pycc::rt::detail {

// Parsed struct item (code and repetition count)
struct StructItem { char code; int count; };

// Pack values from `values_list` according to `items` and endianness into `out`.
// Raises via rt_raise on error (e.g., insufficient values).
void struct_pack_impl(const std::vector<StructItem>& items,
                      bool little,
                      void* values_list,
                      std::vector<unsigned char>& out);

// Unpack bytes `data[0..nb)` according to `items` and endianness into `out_list` (a runtime List).
// Assumes size already validated by the caller; raises on unexpected conditions.
void struct_unpack_impl(const std::vector<StructItem>& items,
                        bool little,
                        const unsigned char* data,
                        std::size_t nb,
                        void*& out_list);

// Compute total byte size for the given parsed items.
// Widths: b/B=1, i/I/f=4. Returns total as 32-bit int.
int32_t struct_calcsize_impl(const std::vector<StructItem>& items);

} // namespace pycc::rt::detail
