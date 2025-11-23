/**
 * @file
 * @brief struct_pack_impl: packing helper for runtime struct module.
 */
#include "runtime/detail/StructHandlers.h"
#include "runtime/detail/RuntimeIntrospection.h"
#include "runtime/Runtime.h"
#include <cstring>
#include <vector>

namespace pycc::rt::detail {

static inline void append_u32(std::vector<unsigned char>& buf, uint32_t v, bool little) {
  if (little) {
    buf.push_back(static_cast<unsigned char>(v & 0xFFU));
    buf.push_back(static_cast<unsigned char>((v >> 8U) & 0xFFU));
    buf.push_back(static_cast<unsigned char>((v >> 16U) & 0xFFU));
    buf.push_back(static_cast<unsigned char>((v >> 24U) & 0xFFU));
  } else {
    buf.push_back(static_cast<unsigned char>((v >> 24U) & 0xFFU));
    buf.push_back(static_cast<unsigned char>((v >> 16U) & 0xFFU));
    buf.push_back(static_cast<unsigned char>((v >> 8U) & 0xFFU));
    buf.push_back(static_cast<unsigned char>(v & 0xFFU));
  }
}

static inline long long to_int_like(void* v) {
  if (!v) return 0;
  switch (type_of_public(v)) {
    case TypeTag::Int: return box_int_value(v);
    case TypeTag::Float: return static_cast<long long>(box_float_value(v));
    case TypeTag::Bool: return box_bool_value(v) ? 1 : 0;
    default: return 0;
  }
}

static inline double to_float_like(void* v) {
  if (!v) return 0.0;
  switch (type_of_public(v)) {
    case TypeTag::Float: return box_float_value(v);
    case TypeTag::Int: return static_cast<double>(box_int_value(v));
    case TypeTag::Bool: return box_bool_value(v) ? 1.0 : 0.0;
    default: return 0.0;
  }
}

void struct_pack_impl(const std::vector<StructItem>& items,
                      bool little,
                      void* values_list,
                      std::vector<unsigned char>& out) {
  std::size_t vi = 0;
  const std::size_t vcount = values_list ? list_len(values_list) : 0;
  auto needValue = [&](const char* what) -> void* {
    if (vi >= vcount) { rt_raise("ValueError", what); return static_cast<void*>(nullptr); }
    return list_get(values_list, vi++);
  };
  for (const auto& it : items) {
    for (int k = 0; k < it.count; ++k) {
      if (it.code == 'b' || it.code == 'B') {
        void* v = needValue("struct.pack: insufficient values"); if (rt_has_exception()) return;
        long long iv = to_int_like(v);
        if (it.code == 'b') {
          if (iv < -128) iv = -128; if (iv > 127) iv = 127;
          out.push_back(static_cast<unsigned char>(iv & 0xFF));
        } else {
          if (iv < 0) iv = 0; if (iv > 255) iv = 255;
          out.push_back(static_cast<unsigned char>(iv));
        }
      } else if (it.code == 'i' || it.code == 'I') {
        void* v = needValue("struct.pack: insufficient values"); if (rt_has_exception()) return;
        uint32_t u = static_cast<uint32_t>(to_int_like(v));
        append_u32(out, u, little);
      } else if (it.code == 'f') {
        void* v = needValue("struct.pack: insufficient values"); if (rt_has_exception()) return;
        float fv = static_cast<float>(to_float_like(v));
        uint32_t u; static_assert(sizeof(float) == 4, "float must be 4 bytes");
        std::memcpy(&u, &fv, sizeof(float));
        append_u32(out, u, little);
      }
    }
  }
}

} // namespace pycc::rt::detail

