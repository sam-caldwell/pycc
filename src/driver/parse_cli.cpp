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

#include <cstring>
#include <iostream>
#include <string>

namespace pycc::driver {

auto ParseCli(int argc, const char* const* argv, CliOptions& dst, std::ostream& err) -> bool {
  // Reset to defaults
  dst = CliOptions{};

  for (int arg_index = 1; arg_index < argc; ++arg_index) {
    const std::string arg = argv[arg_index] ? argv[arg_index] : "";
    if (arg == "-h" || arg == "--help") {
      dst.show_help = true;
      return true;
    } else if (arg == "--metrics") {
      dst.metrics = true;
      dst.metrics_format = CliOptions::MetricsFormat::Text;
    } else if (arg.rfind("--metrics=", 0) == 0) {
      dst.metrics = true;
      const std::string val = arg.substr(std::string("--metrics=").size());
      if (val == "json") {
        dst.metrics_format = CliOptions::MetricsFormat::Json;
      } else if (val == "text") {
        dst.metrics_format = CliOptions::MetricsFormat::Text;
      } else {
        err << "pycc: error: unknown metrics format '" << val << "' (expected json or text)" << '\n';
        return false;
      }
    } else if (arg == "-I") {
      if (arg_index + 1 >= argc) {
        err << "pycc: error: missing path after '-I'" << '\n';
        return false;
      }
      dst.include_dirs.emplace_back(argv[++arg_index]);
    } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
      dst.include_dirs.emplace_back(arg.substr(2));
    } else if (arg == "-L") {
      if (arg_index + 1 >= argc) {
        err << "pycc: error: missing path after '-L'" << '\n';
        return false;
      }
      dst.link_dirs.emplace_back(argv[++arg_index]);
    } else if (arg.rfind("-L", 0) == 0 && arg.size() > 2) {
      dst.link_dirs.emplace_back(arg.substr(2));
    } else if (arg == "-l") {
      if (arg_index + 1 >= argc) {
        err << "pycc: error: missing name after '-l'" << '\n';
        return false;
      }
      dst.link_libs.emplace_back(argv[++arg_index]);
    } else if (arg.rfind("-l", 0) == 0 && arg.size() > 2) {
      dst.link_libs.emplace_back(arg.substr(2));
    } else if (arg == "-o") {
      if (i + 1 >= argc) {
        err << "pycc: error: missing filename after '-o'" << '\n';
        return false;
      }
      dst.output = argv[++i];
    } else if (arg == "-S") {
      dst.emit_asm = true;  // Compile to assembly, do not link
    } else if (arg == "-c") {
      dst.compile_only = true;  // Compile to object, do not link
    } else if (arg == "--") {
      // End of options; rest are positional inputs
      for (++arg_index; arg_index < argc; ++arg_index) {
        dst.inputs.emplace_back(argv[arg_index] ? argv[arg_index] : "");
      }
      break;
    } else if (!arg.empty() && arg[0] == '-') {
      err << "pycc: error: unknown option '" << arg << "'" << '\n';
      return false;
    } else if (!arg.empty()) {
      dst.inputs.push_back(arg);
    }
  }

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
