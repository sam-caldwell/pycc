/***
 * Name: test_codegen_random_lowering
 * Purpose: Verify lowering of random.random/randint/seed.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="randm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenRandom, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  random.seed(123)
  a = random.random()
  b = random.randint(1, 3)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare double @pycc_random_random()"), std::string::npos);
  ASSERT_NE(ir.find("declare i32 @pycc_random_randint(i32, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_random_seed(i64)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_random_seed(i64"), std::string::npos);
  ASSERT_NE(ir.find("call double @pycc_random_random()"), std::string::npos);
  ASSERT_NE(ir.find("call i32 @pycc_random_randint(i32"), std::string::npos);
}

