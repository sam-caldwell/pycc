/***
 * Name: pycc::driver::detail::HandleHelpArg
 * Purpose: Recognize -h/--help and mark show_help.
 * Inputs: current arg, destination options
 * Outputs: Handled when matched, otherwise NotMatched
 * Theory of Operation: No error cases.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"

#include <string>

namespace pycc {
namespace driver {
namespace detail {

auto HandleHelpArg(const std::string& arg, CliOptions& dst) -> OptResult {
  if (arg == "-h" || arg == "--help") {
    dst.show_help = true;
    return OptResult::Handled;
  }
  return OptResult::NotMatched;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc

