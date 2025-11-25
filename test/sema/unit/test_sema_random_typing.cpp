/***
 * Name: test_sema_random_typing
 * Purpose: Ensure Sema types random module functions and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "ra.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaRandom, Accepts) {
  const char* src = R"PY(
def main() -> int:
  import random
  a = random.random()
  b = random.randint(1, 5)
  random.seed(42)
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaRandom, Rejects) {
  const char* wrongArity1 = R"PY(
def main() -> int:
  import random
  a = random.random(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(wrongArity1));

  const char* wrongArity2 = R"PY(
def main() -> int:
  import random
  a = random.randint(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(wrongArity2));

  const char* wrongType = R"PY(
def main() -> int:
  import random
  a = random.seed('x')
  return 0
)PY";
  EXPECT_FALSE(semaOK(wrongType));
}

