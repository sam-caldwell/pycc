/***
 * Name: test_codegen_os_mkdir_casts
 * Purpose: Ensure os.mkdir casts bool/float modes to i32 when provided.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="os_mkdir_casts.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenOS, MkdirModeCasts) {
  const char* src = R"PY(
def main() -> int:
  os.mkdir('d', True)
  os.mkdir('e', 1.5)
  return 0
)PY";
  auto ir = genIR(src);
  // zext i1 -> i32 for bool
  ASSERT_NE(ir.find("zext i1"), std::string::npos);
  // fptosi double -> i32 for float
  ASSERT_NE(ir.find("fptosi double"), std::string::npos);
}

