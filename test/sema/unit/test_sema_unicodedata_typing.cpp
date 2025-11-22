/***
 * Name: test_sema_unicodedata_typing
 * Purpose: Ensure Sema types unicodedata.normalize and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_ud(const char* src) {
  lex::Lexer L; L.pushString(src, "ud.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaUnicodedata, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = unicodedata.normalize('NFC', 'cafe')
  b = unicodedata.normalize('NFD', 'cafe')
  return 0
)PY";
  EXPECT_TRUE(semaOK_ud(src));
}

TEST(SemaUnicodedata, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = unicodedata.normalize(1, 'x')
  return 0
)PY";
  EXPECT_FALSE(semaOK_ud(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = unicodedata.normalize('NFC', 123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_ud(src2));
}

