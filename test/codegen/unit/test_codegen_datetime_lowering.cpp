/***
 * Name: test_codegen_datetime_lowering
 * Purpose: Verify lowering of datetime module API into runtime shims.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="datetime_full.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenDatetime, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = datetime.now()
  b = datetime.utcnow()
  c = datetime.fromtimestamp(0)
  d = datetime.utcfromtimestamp(0)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_datetime_now()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_datetime_utcnow()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_datetime_fromtimestamp(double)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_datetime_utcfromtimestamp(double)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_datetime_now()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_datetime_utcnow()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_datetime_fromtimestamp(double"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_datetime_utcfromtimestamp(double"), std::string::npos);
}
