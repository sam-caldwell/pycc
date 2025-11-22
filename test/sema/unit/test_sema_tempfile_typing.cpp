/***
 * Name: test_sema_tempfile_typing
 * Purpose: Ensure Sema types tempfile.* and rejects arity.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "tmpf.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaTempfile, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = tempfile.gettempdir()
  b = tempfile.mkdtemp()
  c = tempfile.mkstemp()
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaTempfile, RejectsArity) {
  const char* src = R"PY(
def main() -> int:
  a = tempfile.mkdtemp(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

