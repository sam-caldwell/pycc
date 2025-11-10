/***
 * Name: pycc::driver (cli)
 * Purpose: Declarations for CLI options, parsing, and usage printing.
 * Inputs: N/A (declarations only)
 * Outputs: Types and functions for CLI handling.
 * Theory of Operation: Provide a minimal GCC-like CLI for pycc and utilities
 *   to parse arguments and render usage text. Definitions live in .cpp files.
 */
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace pycc {
namespace driver {

/***
 * Name: pycc::driver::CliOptions
 * Purpose: Hold parsed command-line options for a pycc invocation.
 * Inputs: Values are populated by ParseCli.
 * Outputs: Consumed by the driver to control compilation behavior.
 * Theory of Operation: Mirrors a subset of GCC/G++ flags: -o, -S, -c, --help.
 */
struct CliOptions {
  std::vector<std::string> inputs;  // Input source files (.py)
  std::string output = "a.out";     // -o <file>
  bool compile_only = false;        // -c
  bool emit_asm = false;            // -S
  bool show_help = false;           // -h, --help
  bool metrics = false;             // --metrics
  enum class MetricsFormat { Text, Json };
  MetricsFormat metrics_format = MetricsFormat::Text; // --metrics[=json|text]
  // Placeholders for future behavior (parsed only)
  std::vector<std::string> include_dirs;  // -I <dir> or -Idir
  std::vector<std::string> link_dirs;     // -L <dir> or -Ldir
  std::vector<std::string> link_libs;     // -l <lib> or -llib
};

/***
 * Name: pycc::driver::detail::OptResult
 * Purpose: Tri-state result for option handlers.
 * Inputs: N/A
 * Outputs: Indicates whether an argument was handled, not matched, or invalid.
 * Theory of Operation: Allows the main parser to remain simple while delegating
 *   specific option formats to small helpers.
 */
namespace detail {
enum class OptResult { NotMatched, Handled, Error };
}

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
 * Theory of Operation: Iterates arguments left-to-right, handling known flags
 *   and collecting input filenames. Unknown flags cause failure with a message.
 */
bool ParseCli(int argc, const char* const* argv, CliOptions& dst, std::ostream& err);

/***
 * Name: pycc::driver::PrintUsage
 * Purpose: Print CLI usage information for pycc.
 * Inputs:
 *   - out: Destination stream
 *   - argv0: Program name used in usage examples
 * Outputs: None
 * Theory of Operation: Renders a concise, GCC-like usage with supported flags.
 */
void PrintUsage(std::ostream& out, const char* argv0);

}  // namespace driver
}  // namespace pycc
