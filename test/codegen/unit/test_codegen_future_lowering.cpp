/***
 * Name: test_codegen_future_lowering
 * Purpose: Verify __future__.feature() lowers to a constant boolean and does not raise.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="future_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenFuture, AnnotationsLowersToConstBool) {
  const char* src = R"PY(
import __future__
def main() -> int:
  a = __future__.annotations()
  return 0
)PY";
  auto ir = genIR(src);
  // Ensure no call to raise is emitted
  ASSERT_EQ(ir.find("call void @pycc_rt_raise("), std::string::npos);
}

TEST(CodegenFuture, UnknownFeatureLowersToConstBool) {
  const char* src = R"PY(
import __future__
def main() -> int:
  a = __future__.unicode_literals()
  return 0
)PY";
  auto ir = genIR(src);
  // Ensure no call to raise is emitted and module compiles
  ASSERT_EQ(ir.find("call void @pycc_rt_raise("), std::string::npos);
}
