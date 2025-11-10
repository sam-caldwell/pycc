/***
 * Name: pycc::driver::detail::HandleUnknownOrPositional
 * Purpose: Treat tokens beginning with '-' (not matched by previous handlers) as errors;
 *          otherwise, record the token as a positional input.
 * Inputs:
 *   - arg: current argument string
 *   - dst: CLI options destination
 *   - err: error stream
 * Outputs: OptResult::Error for unknown option, OptResult::Handled otherwise.
 * Theory of Operation: This is the last handler evaluated by ParseCli; it ensures
 *   every token is either handled as a positional input or rejected.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"  // direct use of CliOptions

#include <ostream>
#include <string>

namespace pycc {
namespace driver {
namespace detail {

auto HandleUnknownOrPositional(const std::string& arg, CliOptions& dst, std::ostream& err) -> OptResult {
  if (!arg.empty() && arg[0] == '-') {
    err << "pycc: error: unknown option '" << arg << "'" << '\n';
    return OptResult::Error;
  }
  if (!arg.empty()) {
    dst.inputs.push_back(arg);
  }
  return OptResult::Handled;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
