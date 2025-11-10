/***
 * Name: pycc::driver::detail::HandleMetricsArg
 * Purpose: Handle --metrics and --metrics=json|text option.
 * Inputs:
 *   - arg: current argument string
 *   - dst: CLI options destination
 *   - err: error stream
 * Outputs: OptResult indicating match and success/failure
 * Theory of Operation: Recognizes exact "--metrics" and prefix "--metrics=", validates value.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"  // direct use of CliOptions

#include <ostream>
#include <string>
#include <string_view>

namespace pycc {
namespace driver {
namespace detail {

auto HandleMetricsArg(const std::string& arg, CliOptions& dst, std::ostream& err) -> OptResult {
  if (arg == "--metrics") {
    dst.metrics = true;
    dst.metrics_format = CliOptions::MetricsFormat::Text;
    return OptResult::Handled;
  }
  constexpr std::string_view kPrefix{"--metrics="};
  if (arg.rfind(kPrefix, 0) == 0U) {
    dst.metrics = true;
    const std::string value = arg.substr(kPrefix.size());
    if (value == "json") {
      dst.metrics_format = CliOptions::MetricsFormat::Json;
    } else if (value == "text") {
      dst.metrics_format = CliOptions::MetricsFormat::Text;
    } else {
      err << "pycc: error: unknown metrics format '" << value << "' (expected json or text)" << '\n';
      return OptResult::Error;
    }
    return OptResult::Handled;
  }
  return OptResult::NotMatched;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
