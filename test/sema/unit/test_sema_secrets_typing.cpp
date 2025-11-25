/***
 * Name: test_sema_secrets_typing
 * Purpose: Ensure Sema types secrets token functions and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "se.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaSecrets, Accepts) {
  const char* src = R"PY(
def main() -> int:
  import secrets
  a = secrets.token_bytes(16)
  b = secrets.token_hex(8)
  c = secrets.token_urlsafe(8)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaSecrets, RejectsWrongArityAndType) {
  const char* wrongArity = R"PY(
def main() -> int:
  import secrets
  a = secrets.token_bytes()
  return 0
)PY";
  EXPECT_FALSE(semaOK(wrongArity));

  const char* wrongType = R"PY(
def main() -> int:
  import secrets
  a = secrets.token_hex('not-int')
  return 0
)PY";
  EXPECT_FALSE(semaOK(wrongType));
}

