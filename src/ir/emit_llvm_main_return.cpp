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

namespace pycc {
namespace ir {

bool EmitLLVMMainReturnInt(int value, const std::string& module, std::string& out_ir) {
  std::ostringstream ss;
  ss << "; ModuleID = '" << module << "'\n";
  ss << "source_filename = \"" << module << "\"\n\n";
  ss << "define i32 @main() {\n";
  ss << "entry:\n";
  ss << "  ret i32 " << value << "\n";
  ss << "}\n";
  out_ir = ss.str();
  return true;
}

}  // namespace ir
}  // namespace pycc

