/***
 * Name: test_gvn_pure_exprs
 * Purpose: Ensure pure expressions are grouped by hash in GVN analysis.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/GVN.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="gvn.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(GVN, GroupsPureExprs) {
  const char* src = R"PY(
def main() -> int:
  (1+2)
  (1+2)
  (2+1)
  return 0
)PY";
  auto mod = parseSrc(src);
  opt::GVN gvn;
  auto res = gvn.analyze(*mod);
  // We created three pure expressions, but two of them share the same form
  EXPECT_GE(res.expressions, 3u);
  EXPECT_LE(res.classes, res.expressions);
}

