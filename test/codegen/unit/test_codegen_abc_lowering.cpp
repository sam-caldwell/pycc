/***
 * Name: test_codegen_abc_lowering
 * Purpose: Verify lowering and declarations for _abc helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="abc_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenABC, DeclaresAndCalls) {
  const char* src = R"PY(
import _abc
def main() -> int:
  t = _abc.get_cache_token()
  r = _abc.register("A", "B")
  q = _abc.is_registered("A", "B")
  _abc.invalidate_cache()
  _abc.reset()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i64 @pycc_abc_get_cache_token()"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_abc_register(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_abc_is_registered(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_abc_invalidate_cache()"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_abc_reset()"), std::string::npos);
  ASSERT_NE(ir.find("call i64 @pycc_abc_get_cache_token()"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_abc_register(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_abc_is_registered(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_abc_invalidate_cache()"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_abc_reset()"), std::string::npos);
}

