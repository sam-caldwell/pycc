/***
 * Name: pycc::driver::PrintUsage
 * Purpose: Print CLI usage information for pycc.
 * Inputs:
 *   - out: Destination stream
 *   - argv0: Program name used in usage examples
 * Outputs: None
 * Theory of Operation: Renders a concise, GCC-like usage with supported flags.
 */
#include "pycc/driver/cli.h"

#include <cstring>
#include <iostream>
#include <string>

namespace pycc {
namespace driver {

static std::string Basename(const char* path) {
  if (!path || !*path) return "pycc";
  const char* slash = std::strrchr(path, '/');
#ifdef _WIN32
  const char* bslash = std::strrchr(path, '\\');
  if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
  return std::string(slash ? slash + 1 : path);
}

void PrintUsage(std::ostream& out, const char* argv0) {
  const std::string prog = Basename(argv0);
  out << "Usage: " << prog << " [options] file..." << '\n'
      << '\n'
      << "Options:" << '\n'
      << "  -h, --help           Print this help and exit" << '\n'
      << "  -o <file>            Place the output into <file> (default: a.out)" << '\n'
      << "  -S                   Compile only; generate assembly (do not link)" << '\n'
      << "  -c                   Compile and assemble (object file); do not link" << '\n'
      << "  --metrics[=json|text] Print compilation metrics summary (default: text)" << '\n'
      << "  --                    End of options" << '\n'
      << '\n'
      << "Notes:" << '\n'
      << "  - pycc enforces Python 3 type hints and performs inference." << '\n'
      << "  - By default, the build enables emission of LLVM IR and ASM." << '\n';
}

}  // namespace driver
}  // namespace pycc
