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

TEST(SemaBase64, Accepts) {
  const char* src = R"PY(
def main() -> int:
  import base64
  e = base64.b64encode(b'Hi')
  d = base64.b64decode(e)
  e2 = base64.b64encode('Hi')
  d2 = base64.b64decode('aGk=')
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaBase64, RejectsWrongArityAndTypes) {
  const char* wrongArity = R"PY(
def main() -> int:
  import base64
  e = base64.b64encode()
  return 0
)PY";
  EXPECT_FALSE(semaOK(wrongArity));

  const char* wrongType = R"PY(
def main() -> int:
  import base64
  e = base64.b64encode(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK(wrongType));
}

