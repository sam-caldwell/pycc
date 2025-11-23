/**
 * @file
 * @brief argparse_apply_action: handle a single option action.
 */
#include "runtime/detail/ArgparseHandlers.h"
#include "runtime/Runtime.h"
#include <cctype>

namespace pycc::rt::detail {

static inline bool parse_int_string(const std::string& sval, long long& out) {
  if (sval.empty()) return false;
  bool neg = false; std::size_t j = 0; out = 0;
  if (sval[0] == '+' || sval[0] == '-') { neg = (sval[0] == '-'); j = 1; }
  for (; j < sval.size(); ++j) {
    unsigned char uc = static_cast<unsigned char>(sval[j]);
    if (!std::isdigit(uc)) return false;
    out = out * 10 + static_cast<long long>(sval[j] - '0');
  }
  if (neg) out = -out;
  return true;
}

bool argparse_apply_action(const std::string& action_name,
                          void* canon_key,
                          const OptVal& ov,
                          void* args_list,
                          std::size_t& idx,
                          void*& result) {
  if (action_name == "store_true") {
    dict_set(&result, canon_key, box_bool(true));
    return true;
  }

  if (action_name == "store") {
    if (ov.has_val) {
      void* v = string_new(ov.val.data(), ov.val.size());
      dict_set(&result, canon_key, v);
      return true;
    }
    const std::size_t n = args_list ? list_len(args_list) : 0;
    if (idx + 1 >= n) { rt_raise("ValueError", "argparse: missing value"); return false; }
    void* nv = list_get(args_list, ++idx);
    dict_set(&result, canon_key, nv);
    return true;
  }

  if (action_name == "store_int") {
    std::string sval;
    if (ov.has_val) {
      sval = ov.val;
    } else {
      const std::size_t n = args_list ? list_len(args_list) : 0;
      if (idx + 1 >= n) { rt_raise("ValueError", "argparse: missing int value"); return false; }
      void* nv = list_get(args_list, ++idx);
      sval.assign(string_data(nv), string_len(nv));
    }
    long long iv = 0;
    if (!parse_int_string(sval, iv)) { rt_raise("ValueError", "argparse: invalid int"); return false; }
    dict_set(&result, canon_key, box_int(iv));
    return true;
  }

  // Unknown action: ignore
  return true;
}

} // namespace pycc::rt::detail

