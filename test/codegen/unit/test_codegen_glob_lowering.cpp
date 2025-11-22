/***
 * Name: test_codegen_glob_lowering
 * Purpose: Verify lowering of glob module API.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="globmod.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenGlob, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = glob.glob("*.txt")
  b = glob.iglob("**/*.cpp")
  c = glob.escape("a*b?")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_glob_glob(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_glob_iglob(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_glob_escape(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_glob_glob(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_glob_iglob(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_glob_escape(ptr"), std::string::npos);
}

