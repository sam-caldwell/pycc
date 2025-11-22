/***
 * Name: test_sema_random_typing
 * Purpose: Ensure Sema types random.* and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "randm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaRandom, AcceptsCalls) {
  const char* src = R"PY(
def main() -> int:
  random.seed(1)
  a = random.random()
  b = random.randint(1, 3)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaRandom, RejectsArityAndType) {
  const char* src1 = R"PY(
def main() -> int:
  a = random.random(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = random.randint("a", 2)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}

