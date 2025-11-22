/***
 * Name: test_codegen_calendar_lowering
 * Purpose: Verify lowering of calendar.isleap/monthrange.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="cal.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenCalendar, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = calendar.isleap(2024)
  b = calendar.monthrange(2024, 2)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i32 @pycc_calendar_isleap(i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_calendar_monthrange(i32, i32)"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_calendar_isleap(i32"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_calendar_monthrange(i32"), std::string::npos);
}

