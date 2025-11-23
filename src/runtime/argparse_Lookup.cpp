/**
 * @file
 * @brief argparse_lookup_canon: canonicalize option name via optmap.
 */
#include "runtime/detail/ArgparseHandlers.h"
#include "runtime/Runtime.h"

namespace pycc::rt::detail {

void* argparse_lookup_canon(void* optmap, const std::string& opt) {
  if (!optmap) return nullptr;
  void* it = dict_iter_new(optmap);
  for (;;) {
    void* k = dict_iter_next(it);
    if (!k) break;
    std::string ks(string_data(k), string_len(k));
    if (ks == opt) {
      return dict_get(optmap, k);
    }
  }
  return nullptr;
}

} // namespace pycc::rt::detail

