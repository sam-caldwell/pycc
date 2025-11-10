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

namespace pycc::driver {

static std::string_view Basename(const char* path) {
  if (path == nullptr || *path == '\0') {
    return std::string_view{"pycc"};
  }
  const char* last_slash = std::strrchr(path, '/');
#ifdef _WIN32
  const char* last_backslash = std::strrchr(path, '\\');
  if (last_backslash != nullptr && (last_slash == nullptr || last_backslash > last_slash)) {
    last_slash = last_backslash;
  }
#endif
  return std::string_view(last_slash != nullptr ? last_slash + 1 : path);
}

auto PrintUsage(std::ostream& out, const char* argv0) -> void {
  const std::string_view program_name = Basename(argv0);
  out << "Usage: " << program_name << " [options] file..." << '\n'
      << '\n'
      << "Options:" << '\n'
      << "  -h, --help           Print this help and exit" << '\n'
      << "  -o <file>            Place the output into <file> (default: a.out)" << '\n'
      << "  -S                   Compile only; generate assembly (do not link)" << '\n'
      << "  -c                   Compile and assemble (object file); do not link" << '\n'
      << "  -I<dir> | -I <dir>   Add header search path (placeholder)" << '\n'
      << "  -L<dir> | -L <dir>   Add library search path (placeholder)" << '\n'
      << "  -l<lib> | -l <lib>   Link against library name (placeholder)" << '\n'
      << "  --metrics[=json|text] Print compilation metrics summary (default: text)" << '\n'
      << "  --                    End of options" << '\n'
      << '\n'
      << "Notes:" << '\n'
      << "  - pycc enforces Python 3 type hints and performs inference." << '\n'
      << "  - By default, the build enables emission of LLVM IR and ASM." << '\n';
}

}  // namespace pycc::driver
