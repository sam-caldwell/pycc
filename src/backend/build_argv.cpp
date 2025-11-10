/***
 * Name: pycc::backend::detail::BuildArgvMutable
 * Purpose: Construct a null-terminated argv array from a vector<string>.
 * Inputs: args (vector<string>)
 * Outputs: vector<char*> suitable for execvp
 * Theory of Operation: Pointers reference the string storage; ensure lifetime.
 */
#include "pycc/backend/detail/exec.h"

#include <string>
#include <vector>

namespace pycc {
namespace backend {
namespace detail {

auto BuildArgvMutable(std::vector<std::string>& args) -> std::vector<char*> {
  std::vector<char*> argv;
  argv.reserve(args.size() + 1U);
  for (auto& arg_str : args) {
    argv.push_back(const_cast<char*>(arg_str.c_str()));  // NOLINT(cppcoreguidelines-pro-type-const-cast)
  }
  argv.push_back(nullptr);
  return argv;
}

}  // namespace detail
}  // namespace backend
}  // namespace pycc
