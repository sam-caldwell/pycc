/***
 * Name: pycc::driver::PrintUsage
 * Purpose: Print CLI usage information for pycc.
 * Inputs:
 *   - out: Destination stream
 *   - argv0: Program name used in usage examples
 * Outputs: None
 * Theory of Operation: Renders a concise, GCC-like usage with supported flags.
 */
// NOLINTNEXTLINE(misc-include-cleaner) - ensure signature stays in sync
#include "pycc/driver/cli.h"

#include <cstring>
#include <iostream>
#include <string_view>

namespace pycc::driver {

auto PrintUsage(std::ostream& out, const char* argv0) -> void {
  std::string_view program_name{"pycc"};
  if (argv0 != nullptr && *argv0 != '\0') {
    const char* last_slash = std::strrchr(argv0, '/');
#ifdef _WIN32
    const char* last_backslash = std::strrchr(argv0, '\\');
    if (last_backslash != nullptr && (last_slash == nullptr || last_backslash > last_slash)) {
      last_slash = last_backslash;
    }
#endif
    const std::string_view all(argv0);
    std::size_t offset = 0;
    if (last_slash != nullptr) {
      offset = static_cast<std::size_t>(last_slash - argv0) + 1U;
    }
    program_name = all.substr(offset);
  }
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
