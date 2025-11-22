/***
 * Name: test_codegen__asyncio_lowering
 * Purpose: Verify lowering and declarations for _asyncio helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="_asyncio_codegen.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(Codegen_Asyncio, DeclaresAndCalls) {
  const char* src = R"PY(
import _asyncio
def main() -> int:
  loop = _asyncio.get_event_loop()
  fut = _asyncio.Future()
  _asyncio.future_set_result(fut, "x")
  r = _asyncio.future_result(fut)
  d = _asyncio.future_done(fut)
  _asyncio.sleep(0.01)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_asyncio_get_event_loop()"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_asyncio_future_new()"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_asyncio_future_set_result(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_asyncio_future_result(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_asyncio_future_done(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_asyncio_sleep(double)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_asyncio_get_event_loop()"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_asyncio_future_new()"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_asyncio_future_set_result(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_asyncio_future_result(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_asyncio_future_done(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_asyncio_sleep(double"), std::string::npos);
}

