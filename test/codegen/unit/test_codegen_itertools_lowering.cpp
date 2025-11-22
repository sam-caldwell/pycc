/***
 * Name: test_codegen_itertools_lowering
 * Purpose: Verify lowering of itertools materialized helpers and IR declarations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="itertools_lowering.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenItertools, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = [1,2]
  b = [3]
  c = itertools.chain(a, b)
  d = itertools.product(a, a)
  e = itertools.permutations(a)
  f = itertools.zip_longest(a, b)
  g = itertools.islice(a, 0, 2)
  h = itertools.repeat("x", 3)
  i = itertools.chain_from_iterable([[1],[2]])
  return 0
)PY";
  auto ir = genIR(src);
  // Declarations present
  ASSERT_NE(ir.find("declare ptr @pycc_itertools_chain2(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_itertools_product2(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_itertools_permutations(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_itertools_zip_longest2(ptr, ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_itertools_islice(ptr, i32, i32, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_itertools_repeat(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_itertools_chain_from_iterable(ptr)"), std::string::npos);
  // Calls present
  ASSERT_NE(ir.find("call ptr @pycc_itertools_chain2"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_itertools_product2"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_itertools_permutations"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_itertools_zip_longest2"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_itertools_islice"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_itertools_repeat"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_itertools_chain_from_iterable"), std::string::npos);
}

