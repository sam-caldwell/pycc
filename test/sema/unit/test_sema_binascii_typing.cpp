/***
 * Name: test_sema_binascii_typing
 * Purpose: Ensure Sema types binascii.hexlify/unhexlify and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "ba.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaBinascii, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = binascii.hexlify("hi")
  b = binascii.unhexlify("6869")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaBinascii, Rejects) {
  const char* src = R"PY(
def main() -> int:
  a = binascii.hexlify(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

