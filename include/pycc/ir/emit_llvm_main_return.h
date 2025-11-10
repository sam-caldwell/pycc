/***
 * Name: pycc::ir (emit_llvm_main_return)
 * Purpose: Emit minimal LLVM IR for a program whose main returns a constant int.
 * Inputs: Constant int value, module name
 * Outputs: LLVM IR text (.ll) as a string
 * Theory of Operation: Generates a single function `i32 @main()` returning the constant.
 */
#pragma once

#include <string>

namespace pycc::ir {

    /*** EmitLLVMMainReturnInt: Generate LLVM IR. Returns true on success. */
    bool EmitLLVMMainReturnInt(int value, const std::string& module, std::string& out_ir);

}
