/***
 * Name: test_sema_argparse_typing
 * Purpose: Ensure Sema types argparse subset and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_ap(const char* src) {
  lex::Lexer L; L.pushString(src, "ap.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaArgparse, Accepts) {
  const char* src = R"PY(
def main() -> int:
  p = argparse.ArgumentParser()
  argparse.add_argument(p, '--verbose', 'store_true')
  d = argparse.parse_args(p, ['--verbose'])
  return 0
)PY";
  EXPECT_TRUE(semaOK_ap(src));
}

TEST(SemaArgparse, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  p = argparse.ArgumentParser(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK_ap(src1));
  const char* src2 = R"PY(
def main() -> int:
  p = argparse.ArgumentParser()
  argparse.add_argument(p, 1, 'store')
  return 0
)PY";
  EXPECT_FALSE(semaOK_ap(src2));
}

