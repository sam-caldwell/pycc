/***
 * Name: test_codegen_exceptions_unwind
 * Purpose: Verify IR uses invoke/landingpad and runtime helpers for try/raise/except/finally.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src) {
  lex::Lexer L; L.pushString(src, "eh.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenEH, UsesInvokeAndLandingpad) {
  const char* src = R"PY(
def main() -> int:
  try:
    raise ValueError("x")
  except ValueError:
    return 0
  else:
    return 1
  finally:
    x = 0
)PY";
  const auto ir = genIR(src);
  // Personality and landingpad
  ASSERT_NE(ir.find("@__gxx_personality_v0"), std::string::npos);
  ASSERT_NE(ir.find("landingpad"), std::string::npos);
  // Raise lowered via invoke under try
  ASSERT_NE(ir.find("invoke void @pycc_rt_raise"), std::string::npos);
  // Pending exception checks and helpers
  ASSERT_NE(ir.find("@pycc_rt_has_exception"), std::string::npos);
  ASSERT_NE(ir.find("@pycc_rt_current_exception"), std::string::npos);
  ASSERT_NE(ir.find("@pycc_rt_exception_type"), std::string::npos);
  ASSERT_NE(ir.find("@pycc_rt_clear_exception"), std::string::npos);
}

