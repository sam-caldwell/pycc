/**
 * @file
 * @brief json_dump_list: JSON dump handler for runtime List objects.
 */
#include "runtime/detail/JsonHandlers.h"

namespace pycc::rt::detail {

static inline void indent_nl_local(std::string& out, int depth, int indent) {
  out.push_back('\n');
  for (int i = 0; i < depth * indent; ++i) out.push_back(' ');
}

void json_dump_list(void* obj, std::string& out, const DumpOpts& opts, int depth, DumpRecFn rec) {
  auto* meta = reinterpret_cast<std::size_t*>(obj);
  const std::size_t len = meta[0];
  auto** items = reinterpret_cast<void**>(meta + 2);
  out.push_back('[');
  if (len && opts.indent > 0) indent_nl_local(out, depth + 1, opts.indent);
  for (std::size_t i = 0; i < len; ++i) {
    if (i) {
      if (opts.indent > 0) { out.push_back(','); indent_nl_local(out, depth + 1, opts.indent); }
      else if (opts.sepItem) { out += opts.sepItem; }
      else { out.push_back(','); }
    }
    rec(items[i], out, opts, depth + 1);
  }
  if (len && opts.indent > 0) indent_nl_local(out, depth, opts.indent);
  out.push_back(']');
}

} // namespace pycc::rt::detail

