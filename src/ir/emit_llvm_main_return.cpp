/***
 * Name: pycc::ir::EmitLLVMMainReturnInt
 * Purpose: Generate human-readable LLVM IR for an i32 main returning a constant.
 * Inputs:
 *   - value: integer to return
 *   - module: module identifier for the IR
 * Outputs:
 *   - out_ir: resulting LLVM IR text
 * Theory of Operation: Minimal IR with a single function and entry block.
 */
#include "pycc/ir/emit_llvm_main_return.h"

#include <sstream>
#include <string>

namespace pycc {
namespace ir {

bool EmitLLVMMainReturnInt(int value, const std::string& module, std::string& out_ir) {
  std::ostringstream stream;
  stream << "; ModuleID = '" << module << "'\n";
  stream << "source_filename = \"" << module << "\"\n\n";
  stream << "define i32 @main() {\n";
  stream << "entry:\n";
  stream << "  ret i32 " << value << "\n";
  stream << "}\n";
  out_ir = stream.str();
  return true;
}

}  // namespace ir
}  // namespace pycc
