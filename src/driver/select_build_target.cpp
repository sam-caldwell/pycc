/***
 * Name: pycc::driver::SelectBuildTarget
 * Purpose: Choose build kind and output target based on CLI.
 * Inputs:
 *   - opts: CLI options
 *   - out: Derived outputs
 * Outputs:
 *   - pair(BuildKind, targetPath): The action and target path
 * Theory of Operation: -S → AssembleOnly, -c → ObjectOnly, else Link.
 */
#include "pycc/driver/app.h"
#include "pycc/backend/clang_build.h"  // direct use of backend::BuildKind
#include "pycc/driver/cli.h"          // direct use of driver::CliOptions

#include <string>
#include <utility>

namespace pycc::driver {

auto SelectBuildTarget(const driver::CliOptions& opts, const Outputs& out)
    -> std::pair<backend::BuildKind, std::string> {
  if (opts.emit_asm) {
    return {backend::BuildKind::AssembleOnly, out.s};
  }
  if (opts.compile_only) {
    return {backend::BuildKind::ObjectOnly, out.o};
  }
  return {backend::BuildKind::Link, out.bin};
}

}  // namespace pycc::driver
