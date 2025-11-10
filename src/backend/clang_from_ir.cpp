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

#include <cstdlib>
#include <sstream>
#include <string>

namespace pycc::backend {

auto ClangFromIR(const std::string& ir_path,
                 const std::string& output,
                 BuildKind kind,
                 std::string& err,
                 const std::string& clang) -> bool {
  std::ostringstream cmd;
  cmd << clang << ' ';
  if (kind == BuildKind::AssembleOnly) {
    cmd << "-S ";
  }
  if (kind == BuildKind::ObjectOnly) {
    cmd << "-c ";
  }
  cmd << "-o '" << output << "' '" << ir_path << "'";
  const std::string command = cmd.str();
  const int result_code = std::system(command.c_str());
  if (result_code != 0) {
    err = "clang invocation failed (rc=" + std::to_string(result_code) + "): " + command;
    return false;
  }
  return true;
}

}  // namespace pycc::backend
