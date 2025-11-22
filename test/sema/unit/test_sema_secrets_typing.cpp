/***
 * Name: test_sema_secrets_typing
 * Purpose: Ensure Sema types secrets.token_* and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "secm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaSecrets, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = secrets.token_bytes(8)
  b = secrets.token_hex(8)
  c = secrets.token_urlsafe(8)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaSecrets, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = secrets.token_bytes("x")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = secrets.token_hex(1, 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

