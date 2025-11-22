/***
 * Name: test_sema_io_calls
 * Purpose: Ensure Sema accepts io.* calls with correct arg types and rejects invalid arity/types.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "io_sem.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaIO, AcceptsValidCalls) {
  const char* src = R"PY(
def main() -> int:
  io.write_stdout("x")
  io.write_stderr("y")
  c = io.read_file("/dev/null")
  ok = io.write_file("/tmp/pycc-io", "data")
  return 0
)PY";
  ASSERT_TRUE(semaOK(src));
}

TEST(SemaIO, RejectsInvalidArgs) {
  const char* src = R"PY(
def main() -> int:
  io.write_stdout(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

