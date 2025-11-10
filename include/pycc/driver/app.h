/***
 * Name: pycc::driver (app API)
 * Purpose: Declarations for top-level driver helpers used by main().
 * Inputs: CLI options and paths
 * Outputs: Outputs structure, build selections, status codes
 * Theory of Operation: Keep main() minimal by factoring helpers into separate
 *   translation units; adhere to one function/method per .cpp file.
 */
#pragma once

#include <string>
#include <utility>

#include "pycc/backend/clang_build.h"
#include "pycc/driver/cli.h"

namespace pycc {
namespace driver {

/***
 * Name: pycc::driver::Outputs
 * Purpose: Group the derived output file paths based on the requested binary name.
 * Inputs: base output name
 * Outputs: bin (.out), ll (.ll), s (.s), o (.o)
 * Theory of Operation: Computed via DeriveOutputs().
 */
struct Outputs {
  std::string bin;
  std::string ll;
  std::string s;
  std::string o;
};

/***
 * Name: pycc::driver::DeriveOutputs
 * Purpose: Compute related artifact file names from the requested binary name.
 * Inputs: out_bin (requested output binary path)
 * Outputs: Outputs structure with derived names
 * Theory of Operation: Appends appropriate file extensions.
 */
Outputs DeriveOutputs(const std::string& out_bin);

/***
 * Name: pycc::driver::SelectBuildTarget
 * Purpose: Choose the build kind and target path based on CLI switches.
 * Inputs: opts (CLI), out (derived outputs)
 * Outputs: pair of (BuildKind, target path)
 * Theory of Operation: Mirrors gcc/g++ semantics for -S and -c.
 */
std::pair<backend::BuildKind, std::string> SelectBuildTarget(const driver::CliOptions& opts,
                                                             const Outputs& out);

/***
 * Name: pycc::driver::WriteFileOrReport
 * Purpose: Write file and print an error to stderr on failure.
 * Inputs: path, data, err (for detail)
 * Outputs: true on success, false on error (and message printed)
 * Theory of Operation: Wraps support::WriteFile with unified reporting.
 */
bool WriteFileOrReport(const std::string& path, const std::string& data, std::string& err);

/***
 * Name: pycc::driver::ReportMetricsIfRequested
 * Purpose: Emit metrics in the requested format to stdout if enabled.
 * Inputs: opts (CLI options)
 * Outputs: None
 * Theory of Operation: Reads the global Metrics registry and prints either JSON or text.
 */
void ReportMetricsIfRequested(const driver::CliOptions& opts);

/***
 * Name: pycc::driver::CompileOnce
 * Purpose: Execute one end-to-end compile unit from source path to artifacts.
 * Inputs: opts (CLI options), input_path
 * Outputs: POSIX status code (0 success, 2 error)
 * Theory of Operation: Runs staged pipeline (read → frontend → IR → emit).
 */
int CompileOnce(const driver::CliOptions& opts, const std::string& input_path);

}  // namespace driver
}  // namespace pycc

