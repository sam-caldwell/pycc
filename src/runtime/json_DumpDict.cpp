/**
 * @file
 * @brief json_dump_dict: JSON dump handler for runtime Dict objects.
 */
#include "runtime/detail/JsonHandlers.h"
#include "runtime/detail/RuntimeIntrospection.h"
#include <algorithm>

namespace pycc::rt::detail {

static inline void indent_nl_local(std::string& out, int depth, int indent) {
  out.push_back('\n');
  for (int i = 0; i < depth * indent; ++i) out.push_back(' ');
}

void json_dump_dict(void* obj, std::string& out, const DumpOpts& opts, int depth, DumpRecFn rec) {
  // Note: only string keys supported
  auto* base = reinterpret_cast<unsigned char*>(obj);
  auto* pm = reinterpret_cast<std::size_t*>(base);
  const std::size_t cap = pm[1];
  auto** keys = reinterpret_cast<void**>(pm + 3);
  auto** vals = keys + cap;
  out.push_back('{');
  bool first = true;
  if (opts.sortKeys) {
    std::vector<void*> klist; klist.reserve(cap);
    for (std::size_t i = 0; i < cap; ++i) if (keys[i] != nullptr) klist.push_back(keys[i]);
    std::sort(klist.begin(), klist.end(), [](void* a, void* b){
      return std::strcmp(string_data(a), string_data(b)) < 0;
    });
    for (void* kk : klist) {
      // linear search for matching key index (avoids dependency on ptr_hash)
      void* vv = nullptr;
      for (std::size_t i = 0; i < cap; ++i) { if (keys[i] == kk) { vv = vals[i]; break; } }
      if (type_of_public(kk) != TypeTag::String) { rt_raise("TypeError", "json.dumps: dict keys must be str"); out.clear(); return; }
      if (!first) {
        if (opts.indent > 0) { out.push_back(','); indent_nl_local(out, depth + 1, opts.indent); }
        else if (opts.sepItem) { out += opts.sepItem; }
        else { out.push_back(','); }
      }
      first = false;
      // Dump key string via provided recursion callback to ensure proper escaping
      rec(kk, out, opts, depth + 1);
      if (opts.indent > 0) { out.push_back(':'); out.push_back(' '); }
      else if (opts.sepKv) { out += opts.sepKv; }
      else { out.push_back(':'); }
      rec(vv, out, opts, depth + 1);
    }
  } else {
    for (std::size_t i = 0; i < cap; ++i) {
      if (keys[i] != nullptr) {
        if (type_of_public(keys[i]) != TypeTag::String) { rt_raise("TypeError", "json.dumps: dict keys must be str"); out.clear(); return; }
        if (!first) {
          if (opts.indent > 0) { out.push_back(','); indent_nl_local(out, depth + 1, opts.indent); }
          else if (opts.sepItem) { out += opts.sepItem; }
          else { out.push_back(','); }
        }
        first = false;
        rec(keys[i], out, opts, depth + 1);
        if (opts.indent > 0) { out.push_back(':'); out.push_back(' '); }
        else if (opts.sepKv) { out += opts.sepKv; }
        else { out.push_back(':'); }
        rec(vals[i], out, opts, depth + 1);
      }
    }
  }
  if (!first && opts.indent > 0) indent_nl_local(out, depth, opts.indent);
  out.push_back('}');
}

} // namespace pycc::rt::detail
