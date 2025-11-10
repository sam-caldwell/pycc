/***
 * Name: pycc::backend::ClangFromIR
 * Purpose: Use clang to turn LLVM IR (.ll) into assembly/object/binary.
 * Inputs:
 *   - ir_path: path to LLVM IR file
 *   - output: output path for .s/.o/binary depending on kind
 *   - kind: AssembleOnly (-S), ObjectOnly (-c), Link (default)
 *   - clang: clang binary name or path
 * Outputs:
 *   - err: error message on failure
 * Theory of Operation: Constructs a command line and calls std::system.
 */
#include "pycc/backend/clang_build.h"

#include <cstddef>
#include <string>
#include <vector>

#include "pycc/backend/detail/exec.h"

namespace pycc::backend {

auto ClangFromIR(const std::string& ir_path,
                 const std::string& output,
                 const BuildKind kind,
                 std::string& err,
                 const std::string& clang) -> bool {
  std::vector<std::string> args;
  args.emplace_back(clang);
  if (kind == BuildKind::AssembleOnly) {
    args.emplace_back("-S");
  }
  if (kind == BuildKind::ObjectOnly) {
    args.emplace_back("-c");
  }
  args.emplace_back("-o");
  args.emplace_back(output);
  args.emplace_back(ir_path);

  auto argv = detail::BuildArgvMutable(args);
  return detail::ExecAndWait(argv, err);
}

}  // namespace pycc::backend
