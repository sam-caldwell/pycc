/***
 * Name: pycc::driver::detail::NormalizeArgv
 * Purpose: Convert argv into a vector<string> safely (handle null entries).
 * Inputs: argc, argv
 * Outputs: out (vector of strings)
 * Theory of Operation: Reserves and copies with null checks.
 */
#include "pycc/driver/cli_parse.h"

#include <cstddef>
#include <string>
#include <vector>

namespace pycc {
namespace driver {
namespace detail {

void NormalizeArgv(int argc, const char* const* argv, std::vector<std::string>& out) {
  out.clear();
  out.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    const char* arg_ptr = argv[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    out.emplace_back(arg_ptr == nullptr ? "" : arg_ptr);
  }
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
