/***
 * Name: pycc::driver::detail::HandleSwitch
 * Purpose: Handle simple boolean switches: -S and -c.
 * Inputs:
 *   - arg: current argument string
 *   - dst: CLI options destination
 * Outputs: OptResult indicating match and success/failure
 * Theory of Operation: Sets flags and returns Handled when a match occurs.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"  // direct use of CliOptions

#include <string>

namespace pycc {
namespace driver {
namespace detail {

auto HandleSwitch(const std::string& arg, CliOptions& dst) -> OptResult {
  if (arg == "-S") {
    dst.emit_asm = true;
    return OptResult::Handled;
  }
  if (arg == "-c") {
    dst.compile_only = true;
    return OptResult::Handled;
  }
  return OptResult::NotMatched;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
