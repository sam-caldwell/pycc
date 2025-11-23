/**
 * @file
 * @brief struct_unpack_impl: unpacking helper for runtime struct module.
 */
#include "runtime/detail/StructHandlers.h"
#include "runtime/Runtime.h"
#include <cstring>

namespace pycc::rt::detail {

static inline uint32_t read_u32(const unsigned char* p, bool little) {
  if (little) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8U)
         | (static_cast<uint32_t>(p[2]) << 16U) | (static_cast<uint32_t>(p[3]) << 24U);
  }
  return (static_cast<uint32_t>(p[0]) << 24U) | (static_cast<uint32_t>(p[1]) << 16U)
       | (static_cast<uint32_t>(p[2]) << 8U)  | static_cast<uint32_t>(p[3]);
}

void struct_unpack_impl(const std::vector<StructItem>& items,
                        bool little,
                        const unsigned char* data,
                        std::size_t /*nb*/, // size already validated by caller
                        void*& out_list) {
  std::size_t idx = 0;
  for (const auto& it : items) {
    for (int k = 0; k < it.count; ++k) {
      if (it.code == 'b') {
        int8_t v = static_cast<int8_t>(data[idx++]);
        list_push_slot(&out_list, box_int(static_cast<long long>(v)));
      } else if (it.code == 'B') {
        uint8_t v = data[idx++];
        list_push_slot(&out_list, box_int(static_cast<long long>(v)));
      } else if (it.code == 'i' || it.code == 'I') {
        uint32_t u = read_u32(data + idx, little); idx += 4;
        if (it.code == 'i') {
          int32_t s; std::memcpy(&s, &u, sizeof(uint32_t));
          list_push_slot(&out_list, box_int(static_cast<long long>(s)));
        } else {
          list_push_slot(&out_list, box_int(static_cast<long long>(u)));
        }
      } else if (it.code == 'f') {
        uint32_t u = read_u32(data + idx, little); idx += 4;
        float fv; std::memcpy(&fv, &u, sizeof(uint32_t));
        list_push_slot(&out_list, box_float(static_cast<double>(fv)));
      }
    }
  }
}

} // namespace pycc::rt::detail

