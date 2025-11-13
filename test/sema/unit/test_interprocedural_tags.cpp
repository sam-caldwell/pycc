/***
 * Name: test_interprocedural_tags
 * Purpose: Verify basic interprocedural canonical propagation across returns.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Sema, Interprocedural_ReturnsParam_PropagatesCanonical) {
  const char* src =
      "def id(a: int) -> int:\n"
      "  return a\n"
      "def main() -> int:\n"
      "  x = 1\n"
      "  y = id(x)\n"
      "  return y - x\n";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  ASSERT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);

  ASSERT_EQ(mod->functions.size(), 2u);
  const auto& mainFn = *mod->functions[1];
  ASSERT_EQ(mainFn.name, "main");
  ASSERT_EQ(mainFn.body.size(), 3u);
  const auto* s1 = mainFn.body[1].get();
  ASSERT_EQ(s1->kind, ast::NodeKind::AssignStmt);
  const auto* asg = static_cast<const ast::AssignStmt*>(s1);
  ASSERT_TRUE(asg->value);
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Call);
  // The call id(x) should adopt the canonical of its forwarded argument 'x'
  ASSERT_TRUE(asg->value->canonical().has_value());
  EXPECT_EQ(asg->value->canonical().value(), std::string("n:x"));
}

