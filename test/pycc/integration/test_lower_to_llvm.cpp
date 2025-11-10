/***
 * Name: pycc::tests::Integration::LowerToLLVM
 * Purpose: Validate end-to-end lowering from Python source to LLVM IR string.
 * Inputs: none
 * Outputs: Pass/fail test
 * Theory of Operation: Parse `return N` from source, emit IR, assert `ret i32 N`.
 */
#include <gtest/gtest.h>

#include <string>

#include "pycc/frontend/simple_return_int.h"
#include "pycc/ir/emit_llvm_main_return.h"

using namespace pycc;

TEST(LowerToLLVM, ReturnConstant) {
  const std::string src = "def main() -> int:\n    return 123\n";
  int value = 0;
  std::string err;
  ASSERT_TRUE(frontend::ParseReturnInt(src, value, err)) << err;
  EXPECT_EQ(123, value);

  std::string ir;
  ASSERT_TRUE(ir::EmitLLVMMainReturnInt(value, "module", ir));
  EXPECT_NE(std::string::npos, ir.find("ret i32 123")) << ir;
}

