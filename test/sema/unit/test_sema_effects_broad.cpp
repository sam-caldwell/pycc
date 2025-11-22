/***
 * Name: test_sema_effects_broad
 * Purpose: Exercise effects scanning across many node kinds to improve coverage.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="effects_broad.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaEffectsBroad, VariousStatementsMayRaise) {
  const char* src = R"PY(
def main() -> int:
  x = 0
  a = x.y           # attribute access
  b = math.sqrt(1)  # call
  l = [1]
  c = l[0]          # subscript
  d = 1 + 2         # binary add
  if 1/0:           # division in condition
    pass
  else:
    pass
  while 1/0:        # division in loop condition
    break
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
  const auto& body = mod->functions[0]->body;
  ASSERT_GE(body.size(), 10u);
  // attribute assign
  EXPECT_TRUE(S.mayRaise(body[1].get()));
  // call assign
  EXPECT_TRUE(S.mayRaise(body[2].get()));
  // subscript assign
  EXPECT_TRUE(S.mayRaise(body[4].get()));
  // simple add assign should not raise
  EXPECT_FALSE(S.mayRaise(body[5].get()));
  // if statement registered as may-raise due to division in condition
  EXPECT_TRUE(S.mayRaise(body[6].get()));
  // while statement registered as may-raise
  EXPECT_TRUE(S.mayRaise(body[8].get()));
}

TEST(SemaEffectsBroad, AddDiagIsExercisedByError) {
  const char* src = R"PY(
def main() -> int:
  l = [1]
  x = l['not-int']  # invalid subscript index type
  return 0
)PY";
  auto mod = parseSrc(src, "effects_error.py");
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
  ASSERT_FALSE(diags.empty());
}
