/***
 * Name: pycc::driver::detail::HandleEndOfOptions
 * Purpose: Handle the "--" token and push remaining inputs.
 * Inputs:
 *   - args: full argument vector
 *   - index: current index (will be advanced to the end)
 *   - argc: total argument count
 *   - dst: CLI options destination
 * Outputs: OptResult indicating match and success/failure
 * Theory of Operation: Consumes "--" and appends all remaining tokens as inputs.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"  // direct use of CliOptions

#include <cstddef>
#include <string>
#include <vector>

namespace pycc {
namespace driver {
namespace detail {

auto HandleEndOfOptions(const std::vector<std::string>& args,
                        int& index,
                        int argc,
                        CliOptions& dst) -> OptResult {
  const std::string& arg = args[static_cast<std::size_t>(index)];
  if (arg != "--") {
    return OptResult::NotMatched;
  }
  for (++index; index < argc; ++index) {
    dst.inputs.emplace_back(args[static_cast<std::size_t>(index)]);
  }
  return OptResult::Handled;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
