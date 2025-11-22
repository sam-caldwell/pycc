/***
 * Name: test_codegen_heapq_lowering
 * Purpose: Verify lowering of heapq.heappush/heappop.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="hpq.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenHeapq, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  import heapq
  a = [3,1,4]
  heapq.heappush(a, 2)
  x = heapq.heappop(a)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare void @pycc_heapq_heappush(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_heapq_heappop(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_heapq_heappush(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_heapq_heappop(ptr"), std::string::npos);
}

