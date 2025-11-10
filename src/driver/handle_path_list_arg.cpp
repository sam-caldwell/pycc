/***
 * Name: pycc::driver::detail::HandlePathListArg
 * Purpose: Handle list-style flags with optional conjoined values: -X<val> or -X <val>.
 * Inputs:
 *   - arg: current argument string
 *   - short_opt: option name (e.g., "-I")
 *   - args: full argument vector
 *   - index: current index (will be advanced if value is consumed from next arg)
 *   - argc: total argument count
 *   - out: list to append the parsed value
 *   - missing_msg: error message to emit if next arg missing
 *   - err: error stream
 * Outputs: OptResult indicating match and success/failure
 * Theory of Operation: Supports both conjoined and spaced forms in a single helper.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"  // direct use of CliOptions

#include <cstddef>
#include <string>
#include <vector>

namespace pycc {
namespace driver {
namespace detail {

auto HandlePathListArg(const std::string& arg, const PathListParams& params) -> OptResult {
  if (arg.rfind(params.short_opt, 0) != 0U) {
    return OptResult::NotMatched;
  }
  if (arg.size() > params.short_opt.size()) {
    params.out.emplace_back(arg.substr(params.short_opt.size()));
    return OptResult::Handled;
  }
  if (params.index + 1 >= params.argc) {
    params.err << "pycc: error: " << params.missing_msg << '\n';
    return OptResult::Error;
  }
  ++params.index;
  params.out.emplace_back(params.args[static_cast<std::size_t>(params.index)]);
  return OptResult::Handled;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
