/**
 * @file
 * @brief argparse_split_optval: split "--opt=value" tokens.
 */
#include "runtime/detail/ArgparseHandlers.h"

namespace pycc::rt::detail {

OptVal argparse_split_optval(const std::string& token) {
  OptVal ov;
  std::size_t eq = token.find('=');
  if (eq == std::string::npos) {
    ov.opt = token;
    ov.has_val = false;
  } else {
    ov.opt = token.substr(0, eq);
    ov.val = token.substr(eq + 1);
    ov.has_val = true;
  }
  return ov;
}

} // namespace pycc::rt::detail

