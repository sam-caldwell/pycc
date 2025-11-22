/***
 * Name: test_licm_safety
 * Purpose: Verify LICM hoists only safe invariants and avoids unsafe cases.
 */
#include <gtest/gtest.h>
#include "optimizer/LICM.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="licm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L); return P.parseModule();
}

TEST(LICM, HoistsPureIndependentAssign) {
  const char* src = R"PY(
def f() -> int:
  while 1:
    x = 1 + 2
    break
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::LICM licm; auto n = licm.run(*mod);
  EXPECT_GE(n, 1u);
}

TEST(LICM, NoHoist_MultipleWritesOfTarget) {
  const char* src = R"PY(
def g() -> int:
  while 1:
    x = 1
    x = 2
    break
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::LICM licm; auto n = licm.run(*mod);
  EXPECT_EQ(n, 0u);
}

TEST(LICM, NoHoist_RHSDependsOnLoopWrite) {
  const char* src = R"PY(
def h() -> int:
  while 1:
    y = 1
    y = 2
    x = y + 2
    break
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::LICM licm; auto n = licm.run(*mod);
  EXPECT_EQ(n, 0u);
}

TEST(LICM, NoHoist_ReadBeforeWriteInLoop) {
  const char* src = R"PY(
def k() -> int:
  while 1:
    y = x + 1
    x = 2
    break
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::LICM licm; auto n = licm.run(*mod);
  EXPECT_EQ(n, 0u);
}

TEST(LICM, NoHoist_ImpureCallOnRHS) {
  const char* src = R"PY(
def call() -> int:
  return 1
def m() -> int:
  while 1:
    x = call()
    break
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::LICM licm; auto n = licm.run(*mod);
  EXPECT_EQ(n, 0u);
}
