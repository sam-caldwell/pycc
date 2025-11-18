/***
 * Name: test_sema_opt_stability
 * Purpose: Ensure Sema remains valid after running optimization passes.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"
#include "optimizer/ConstantFold.h"
#include "optimizer/AlgebraicSimplify.h"
#include "optimizer/DCE.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseMod(const char* src) {
  lex::Lexer L; L.pushString(src, "sema_opt.py");
  parse::Parser P(L);
  return P.parseModule();
}

static void runSemaTwiceWithOpts(ast::Module& m) {
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  ASSERT_TRUE(S.check(m, diags)) << (diags.empty() ? "" : diags[0].message);

  opt::ConstantFold cf; (void)cf.run(m);
  opt::AlgebraicSimplify alg; (void)alg.run(m);
  opt::DCE dce; (void)dce.run(m);

  diags.clear();
  EXPECT_TRUE(S.check(m, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(SemaOptStability, Arithmetic) {
  const char* src = R"PY(
def main() -> int:
  y = (2 + 3) * 4
  return y
)PY";
  auto mod = parseMod(src);
  runSemaTwiceWithOpts(*mod);
}

TEST(SemaOptStability, BooleanShortCircuitToInt) {
  const char* src = R"PY(
def main() -> int:
  a = True
  b = False
  c = (a and b) or (not b)
  return 1 if c else 0
)PY";
  auto mod = parseMod(src);
  runSemaTwiceWithOpts(*mod);
}

TEST(SemaOptStability, ComparisonAndIf) {
  const char* src = R"PY(
def main() -> int:
  if (3 * 3) >= (2 + 7):
    return 1
  else:
    return 0
)PY";
  auto mod = parseMod(src);
  runSemaTwiceWithOpts(*mod);
}

