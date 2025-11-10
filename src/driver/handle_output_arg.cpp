/***
 * Name: pycc::driver::detail::HandleOutputArg
 * Purpose: Handle the -o <file> option.
 * Inputs:
 *   - args: full argument vector
 *   - index: current index (will be advanced to consume the filename)
 *   - argc: total argument count
 *   - dst: CLI options destination
 *   - err: error stream
 * Outputs: OptResult indicating match and success/failure
 * Theory of Operation: Look for "-o" and then require a following filename.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"  // direct use of CliOptions

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

namespace pycc {
namespace driver {
namespace detail {

auto HandleOutputArg(const std::vector<std::string>& args,
                     int& index,
                     int argc,
                     CliOptions& dst,
                     std::ostream& err) -> OptResult {
  const std::string& arg = args[static_cast<std::size_t>(index)];
  if (arg != "-o") {
    return OptResult::NotMatched;
  }
  if (index + 1 >= argc) {
    err << "pycc: error: missing filename after '-o'" << '\n';
    return OptResult::Error;
  }
  ++index;
  dst.output = args[static_cast<std::size_t>(index)];
  return OptResult::Handled;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
