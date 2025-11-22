/***
 * Name: test_exceptions_shapes_more
 * Purpose: Add more try/except/finally IR shape checks (finally-only, multiple excepts, nested try).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string irFor(const char* src, const char* file="eh_more.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenIR_EHMore, TryFinallyOnly) {
  const char* src = R"PY(
def main() -> int:
  try:
    x = 1
  finally:
    y = 2
  return 0
)PY";
  auto ir = irFor(src);
  // Should still use personality and landingpad infrastructure
  ASSERT_NE(ir.find("@__gxx_personality_v0"), std::string::npos);
  ASSERT_NE(ir.find("landingpad"), std::string::npos);
}

TEST(CodegenIR_EHMore, MultipleExceptsChooseFirstMatch) {
  const char* src = R"PY(
def main() -> int:
  try:
    raise ValueError("x")
  except ValueError:
    return 1
  except Exception:
    return 2
)PY";
  auto ir = irFor(src);
  // Matching logic should query exception type and string compare helper; invoke should be present
  ASSERT_NE(ir.find("invoke void @pycc_rt_raise"), std::string::npos);
  ASSERT_NE(ir.find("@pycc_rt_exception_type"), std::string::npos);
  ASSERT_NE(ir.find("@pycc_string_eq"), std::string::npos);
}

TEST(CodegenIR_EHMore, NestedTryFinally) {
  const char* src = R"PY(
def main() -> int:
  try:
    try:
      raise Exception("e")
    finally:
      x = 0
  except Exception:
    return 3
)PY";
  auto ir = irFor(src);
  ASSERT_NE(ir.find("landingpad"), std::string::npos);
  ASSERT_NE(ir.find("invoke void @pycc_rt_raise"), std::string::npos);
}

