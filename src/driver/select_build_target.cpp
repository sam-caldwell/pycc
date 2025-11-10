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

namespace pycc {
namespace driver {

std::pair<backend::BuildKind, std::string> SelectBuildTarget(const driver::CliOptions& opts,
                                                             const Outputs& out) {
  if (opts.emit_asm) return {backend::BuildKind::AssembleOnly, out.s};
  if (opts.compile_only) return {backend::BuildKind::ObjectOnly, out.o};
  return {backend::BuildKind::Link, out.bin};
}

}  // namespace driver
}  // namespace pycc

