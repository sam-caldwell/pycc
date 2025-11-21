/***
 * Name: test_codegen_concurrency_builtins
 * Purpose: Verify lowering of spawn/join and channel builtins into runtime calls and wrapper emission.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="conc.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenConcurrency, SpawnJoinAndChannelsLowered) {
  const char* src = R"PY(
def worker() -> int:
  return 0
def main() -> int:
  h = spawn(worker)
  join(h)
  c = chan_new(1)
  chan_send(c, 7)
  v = chan_recv(c)
  return 0
)PY";
  const auto ir = genIR(src);
  // Declarations present
  ASSERT_NE(ir.find("declare ptr @pycc_rt_spawn"), std::string::npos);
  ASSERT_NE(ir.find("declare i1 @pycc_rt_join"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_chan_new"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_chan_send"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_chan_recv"), std::string::npos);
  // Wrapper and calls present
  ASSERT_NE(ir.find("define void @__pycc_start_worker"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_rt_spawn(ptr @__pycc_start_worker, ptr null, i64 0)"), std::string::npos);
  ASSERT_NE(ir.find("call i1 @pycc_rt_join(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_chan_new(i64 1)"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_chan_send(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_chan_recv(ptr"), std::string::npos);
}

