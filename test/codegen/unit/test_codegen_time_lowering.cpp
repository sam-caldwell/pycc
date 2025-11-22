/***
 * Name: test_codegen_time_lowering
 * Purpose: Verify lowering of time module API.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="time_full.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenTime, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = time.time()
  b = time.time_ns()
  c = time.monotonic()
  d = time.perf_counter()
  e = time.process_time()
  time.sleep(0.001)
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare double @pycc_time_time()"), std::string::npos);
  ASSERT_NE(ir.find("declare i64 @pycc_time_time_ns()"), std::string::npos);
  ASSERT_NE(ir.find("call double @pycc_time_time()"), std::string::npos);
  ASSERT_NE(ir.find("call i64 @pycc_time_time_ns()"), std::string::npos);
  ASSERT_NE(ir.find("declare double @pycc_time_monotonic()"), std::string::npos);
  ASSERT_NE(ir.find("declare double @pycc_time_perf_counter()"), std::string::npos);
  ASSERT_NE(ir.find("declare double @pycc_time_process_time()"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_time_sleep(double)"), std::string::npos);
}

TEST(CodegenTime, NsAndProcessCallsPresent) {
  const char* src = R"PY(
def main() -> int:
  a = time.monotonic_ns()
  b = time.perf_counter_ns()
  c = time.process_time()
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare i64 @pycc_time_monotonic_ns()"), std::string::npos);
  ASSERT_NE(ir.find("declare i64 @pycc_time_perf_counter_ns()"), std::string::npos);
  ASSERT_NE(ir.find("declare double @pycc_time_process_time()"), std::string::npos);
  ASSERT_NE(ir.find("call i64 @pycc_time_monotonic_ns()"), std::string::npos);
  ASSERT_NE(ir.find("call i64 @pycc_time_perf_counter_ns()"), std::string::npos);
  ASSERT_NE(ir.find("call double @pycc_time_process_time()"), std::string::npos);
}
