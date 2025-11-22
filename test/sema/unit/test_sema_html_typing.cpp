/***
 * Name: test_sema_html_typing
 * Purpose: Ensure Sema types html.escape/unescape and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "htmlm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaHtml, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = html.escape("<&>")
  b = html.escape("'\"", 1)
  c = html.unescape("&amp;&lt;&gt;")
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaHtml, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  a = html.escape(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = html.unescape(2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

