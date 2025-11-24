/**
 * @file
 * @brief Decode helpers for utf-8 and ascii.
 */
#include "runtime/detail/EncodingHandlers.h"
#include "runtime/Runtime.h"

namespace pycc::rt::detail {

static inline bool is_utf8_cont(unsigned char c) { return (c & 0xC0U) == 0x80U; }

bool decode_utf8_bytes(const unsigned char* p, std::size_t nb, const char* errors, std::string& out_utf8) {
  if (utf8_is_valid(reinterpret_cast<const char*>(p), nb)) {
    out_utf8.assign(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p) + nb);
    return true;
  }
  if (errors && std::strcmp(errors, "replace") == 0) {
    out_utf8.clear(); out_utf8.reserve(nb);
    for (std::size_t i=0; i<nb; ) {
      unsigned char c = p[i];
      if ((c & 0x80U) == 0) { out_utf8.push_back(static_cast<char>(c)); ++i; }
      else if ((c & 0xE0U) == 0xC0U && (i+1)<nb && is_utf8_cont(p[i+1])) { out_utf8.push_back(static_cast<char>(c)); out_utf8.push_back(static_cast<char>(p[i+1])); i+=2; }
      else if ((c & 0xF0U) == 0xE0U && (i+2)<nb && is_utf8_cont(p[i+1]) && is_utf8_cont(p[i+2])) { out_utf8.push_back(static_cast<char>(c)); out_utf8.push_back(static_cast<char>(p[i+1])); out_utf8.push_back(static_cast<char>(p[i+2])); i+=3; }
      else if ((c & 0xF8U) == 0xF0U && (i+3)<nb && is_utf8_cont(p[i+1]) && is_utf8_cont(p[i+2]) && is_utf8_cont(p[i+3])) { out_utf8.push_back(static_cast<char>(c)); out_utf8.push_back(static_cast<char>(p[i+1])); out_utf8.push_back(static_cast<char>(p[i+2])); out_utf8.push_back(static_cast<char>(p[i+3])); i+=4; }
      else { out_utf8.push_back(static_cast<char>(0xEFU)); out_utf8.push_back(static_cast<char>(0xBFU)); out_utf8.push_back(static_cast<char>(0xBDU)); ++i; }
    }
    return true;
  }
  return false;
}

bool decode_ascii_bytes(const unsigned char* p, std::size_t nb, const char* errors, std::string& out_ascii) {
  for (std::size_t i=0; i<nb; ++i) {
    if ((p[i] & 0x80U) != 0) {
      if (errors && std::strcmp(errors, "replace") == 0) {
        out_ascii.resize(nb);
        for (std::size_t j=0; j<nb; ++j) out_ascii[j] = ((p[j] & 0x80U) ? '?' : static_cast<char>(p[j]));
        return true;
      }
      return false;
    }
  }
  out_ascii.assign(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p) + nb);
  return true;
}

} // namespace pycc::rt::detail

