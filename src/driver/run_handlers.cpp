/***
 * Name: pycc::driver::detail::RunHandlers
 * Purpose: Execute the ordered handler list for argument at 'index'.
 * Inputs: args, index (in/out), argc, dst, err
 * Outputs: OptResult (Error halts, Handled continues)
 * Theory of Operation: Table-driven dispatch; last handler captures unknown/positional.
 */
#include "pycc/driver/cli_parse.h"
#include "pycc/driver/cli.h"

#include <array>
#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace pycc {
namespace driver {
namespace detail {

auto RunHandlers(const std::vector<std::string>& args,
                 int& index,
                 int argc,
                 CliOptions& dst,
                 std::ostream& err) -> OptResult {
  using OptResult = detail::OptResult;
  using HandlerFn = std::function<OptResult(int&)>;

  const std::array handlers{
      HandlerFn{[&](int& idx) { return HandleHelpArg(args[static_cast<std::size_t>(idx)], dst); }},
      HandlerFn{[&](int& idx) { return HandleMetricsArg(args[static_cast<std::size_t>(idx)], dst, err); }},
      HandlerFn{[&](int& idx) {
        const std::string& current = args[static_cast<std::size_t>(idx)];
        const PathListParams params{"-I", args, idx, argc, dst.include_dirs, "missing path after '-I'", err};
        return HandlePathListArg(current, params);
      }},
      HandlerFn{[&](int& idx) {
        const std::string& current = args[static_cast<std::size_t>(idx)];
        const PathListParams params{"-L", args, idx, argc, dst.link_dirs, "missing path after '-L'", err};
        return HandlePathListArg(current, params);
      }},
      HandlerFn{[&](int& idx) {
        const std::string& current = args[static_cast<std::size_t>(idx)];
        const PathListParams params{"-l", args, idx, argc, dst.link_libs, "missing name after '-l'", err};
        return HandlePathListArg(current, params);
      }},
      HandlerFn{[&](int& idx) { return HandleOutputArg(args, idx, argc, dst, err); }},
      HandlerFn{[&](int& idx) {
        (void)idx;  // index unchanged for switches
        return HandleSwitch(args[static_cast<std::size_t>(index)], dst);
      }},
      HandlerFn{[&](int& idx) { return HandleEndOfOptions(args, idx, argc, dst); }},
      HandlerFn{[&](int& idx) {
        (void)idx;
        return HandleUnknownOrPositional(args[static_cast<std::size_t>(index)], dst, err);
      }},
  };

  for (const auto& handler : handlers) {
    const OptResult result = handler(index);
    if (result == OptResult::Error) {
      return OptResult::Error;
    }
    if (result == OptResult::Handled) {
      return OptResult::Handled;
    }
  }
  return OptResult::NotMatched;
}

}  // namespace detail
}  // namespace driver
}  // namespace pycc
