/**
 * @file
 * @brief Base64 decode implementation.
 */
#include "runtime/detail/Base64Handlers.h"

namespace pycc::rt::detail {

static inline int b64val(int c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62; if (c == '/') return 63; return -1;
}

void base64_decode_bytes(const unsigned char* data, std::size_t len, std::string& out) {
  // compact copy skipping ASCII whitespace
  std::string in; in.reserve(len);
  for (std::size_t i = 0; i < len; ++i) {
    unsigned char c = data[i];
    if (!(c=='\n' || c=='\r' || c=='\t' || c==' ')) in.push_back(static_cast<char>(c));
  }
  out.reserve((in.size() * 3) / 4);
  std::size_t i = 0;
  while (i < in.size()) {
    int a = (i < in.size()) ? b64val(in[i++]) : -1;
    int b = (i < in.size()) ? b64val(in[i++]) : -1;
    int c = (i < in.size()) ? (in[i] == '=' ? -2 : b64val(in[i])) : -1; if (i < in.size()) ++i;
    int d = (i < in.size()) ? (in[i] == '=' ? -2 : b64val(in[i])) : -1; if (i < in.size()) ++i;
    if (a < 0 || b < 0) break;
    unsigned int n = (static_cast<unsigned int>(a) << 18) | (static_cast<unsigned int>(b) << 12);
    out.push_back(static_cast<char>((n >> 16) & 0xFF));
    if (c == -2) break;
    if (c < 0) break;
    n |= (static_cast<unsigned int>(c) << 6);
    out.push_back(static_cast<char>((n >> 8) & 0xFF));
    if (d == -2) break;
    if (d < 0) break;
    n |= static_cast<unsigned int>(d);
    out.push_back(static_cast<char>(n & 0xFF));
  }
}

} // namespace pycc::rt::detail

