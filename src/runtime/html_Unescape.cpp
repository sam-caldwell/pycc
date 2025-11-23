/**
 * @file
 * @brief html_unescape_impl: Decode HTML entities in a byte range.
 */
#include "runtime/detail/HtmlHandlers.h"
#include <cstring>

namespace pycc::rt::detail {

static inline int from_hex_digit_local(char c) {
  if (c>='0'&&c<='9') return c-'0';
  if (c>='a'&&c<='f') return 10+(c-'a');
  if (c>='A'&&c<='F') return 10+(c-'A');
  return -1;
}

void html_unescape_impl(const char* d, std::size_t n, std::string& out) {
  out.clear(); out.reserve(n);
  for (std::size_t i = 0; i < n;) {
    char c = d[i];
    if (c == '&') {
      if (i+5<=n && std::memcmp(d+i, "&amp;", 5)==0) { out.push_back('&'); i+=5; continue; }
      if (i+4<=n && std::memcmp(d+i, "&lt;", 4)==0) { out.push_back('<'); i+=4; continue; }
      if (i+4<=n && std::memcmp(d+i, "&gt;", 4)==0) { out.push_back('>'); i+=4; continue; }
      if (i+6<=n && std::memcmp(d+i, "&quot;", 6)==0) { out.push_back('"'); i+=6; continue; }
      if (i+6<=n && std::memcmp(d+i, "&#x27;", 6)==0) { out.push_back('\''); i+=6; continue; }
      if (i+3<n && d[i+1]=='#') {
        std::size_t j = i+2; int base = 10; if (j<n && (d[j]=='x' || d[j]=='X')) { base = 16; ++j; }
        int val = 0; bool ok=false;
        for (; j<n && d[j] != ';'; ++j) {
          if (base==10) { if (!(d[j]>='0'&&d[j]<='9')) break; ok=true; val = val*10 + (d[j]-'0'); }
          else { int hv = from_hex_digit_local(d[j]); if (hv<0) break; ok=true; val = val*16 + hv; }
        }
        if (ok && j<n && d[j]==';') { out.push_back(static_cast<char>(val)); i = j+1; continue; }
      }
    }
    out.push_back(c); ++i;
  }
}

} // namespace pycc::rt::detail

