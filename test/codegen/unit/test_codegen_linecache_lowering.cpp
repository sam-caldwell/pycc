/***
 * Name: test_codegen_linecache_lowering
 * Purpose: Verify lowering of linecache.getline.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="lc.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenLinecache, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = linecache.getline("x.txt", 2)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_linecache_getline(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_linecache_getline(ptr"), std::string::npos);
}

