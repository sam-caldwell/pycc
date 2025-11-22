/***
 * Name: test_sema_calendar_typing
 * Purpose: Ensure Sema types calendar.isleap/monthrange and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "cal.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaCalendar, Accepts) {
  const char* src = R"PY(
def main() -> int:
  a = calendar.isleap(2024)
  b = calendar.monthrange(2024, 2)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaCalendar, Rejects) {
  const char* src = R"PY(
def main() -> int:
  a = calendar.isleap("y")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

