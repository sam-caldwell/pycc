/***
 * Name: pycc::driver::ParseCli
 * Purpose: Parse command-line arguments into a CliOptions structure.
 * Inputs:
 *   - argc: Argument count
 *   - argv: Argument vector
 *   - dst: Output options structure to populate
 *   - err: Stream for diagnostics on parse errors
 * Outputs:
 *   - bool: true on successful parse, false if an error occurs
 * Theory of Operation:
 *   Implements a minimal GCC-like parser supporting -h/--help, -o <file>,
 *   -S (assembly only), -c (compile only), and positional input files.
 */
#include "pycc/driver/cli.h"
#include "pycc/driver/cli_parse.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace pycc::driver {

auto ParseCli(const int argc, const char* const* argv, CliOptions& dst, std::ostream& err) -> bool {
  // Reset to defaults
  dst = CliOptions{};

  // Normalize argv into a vector of strings to avoid pointer arithmetic.
  std::vector<std::string> args;
  detail::NormalizeArgv(argc, argv, args);

  for (int arg_index = 1; arg_index < argc; ++arg_index) {
    const std::string& arg = args[static_cast<std::size_t>(arg_index)];
    const detail::OptResult result = detail::RunHandlers(args, arg_index, argc, dst, err);
    if (result == detail::OptResult::Error) {
      return false;
    }
  }

  // ReSharper disable once CppDFAConstantConditions
  if (!dst.show_help && dst.inputs.empty()) {
    err << "pycc: error: no input files" << '\n';
    return false;
  }

  // -S implies no link; in our model that's mutually exclusive with -c link stage too.
  if (dst.emit_asm) {
    dst.compile_only = true;  // Treat as compile-only mode
  }
  return true;
}

}  // namespace pycc::driver
