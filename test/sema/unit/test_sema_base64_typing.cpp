/***
 * Name: test_sema_base64_typing
 * Purpose: Ensure Sema types base64.b64encode/b64decode and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "b64.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaBase64, AcceptsStrAndBytes) {
  const char* src = R"PY(
def main() -> int:
  a = base64.b64encode("hi")
  b = base64.b64decode(a)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaBase64, RejectsWrongArityAndType) {
  const char* src1 = R"PY(
def main() -> int:
  a = base64.b64encode(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = base64.b64decode("a", "b")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

