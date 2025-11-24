/***
 * Name: test_codegen_time_sleep_casts
 * Purpose: Ensure time.sleep casts int/bool to double.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="time_sleep_casts.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenTime, SleepCastsIntAndBoolToDouble) {
  const char* src = R"PY(
def main() -> int:
  time.sleep(1)
  time.sleep(True)
  return 0
)PY";
  auto ir = genIR(src);
  // sitofp for int
  ASSERT_NE(ir.find("sitofp i32"), std::string::npos);
  // uitofp for bool
  ASSERT_NE(ir.find("uitofp i1"), std::string::npos);
}

