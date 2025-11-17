/***
 * Name: test_parser_comprehensions_generators
 * Purpose: Ensure list/set/dict comprehensions and generator expressions support multi-for and multi-if guards,
 *          and handle destructuring targets.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Comprehension.h"
#include "ast/TupleLiteral.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "compgen.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserCompGen, ListCompMultiForAndIf) {
  const char* src =
      "def main() -> int:\n"
      "  a = [(i, j) for i in [1,2] if i for j in [3,4] if j]\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::ListComp);
  const auto* lc = static_cast<const ast::ListComp*>(asg->value.get());
  ASSERT_EQ(lc->fors.size(), 2u);
  EXPECT_EQ(lc->fors[0].ifs.size(), 1u);
  EXPECT_EQ(lc->fors[1].ifs.size(), 1u);
}

TEST(ParserCompGen, DictCompDestructureTargetAndIf) {
  const char* src =
      "def main() -> int:\n"
      "  d = {k: v for (k, v) in [(1,2), (3,4)] if k}\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::DictComp);
  const auto* dc = static_cast<const ast::DictComp*>(asg->value.get());
  ASSERT_EQ(dc->fors.size(), 1u);
  ASSERT_EQ(dc->fors[0].ifs.size(), 1u);
  ASSERT_EQ(dc->fors[0].target->kind, ast::NodeKind::TupleLiteral);
}

TEST(ParserCompGen, GeneratorMultiForAndIf) {
  const char* src =
      "def main() -> int:\n"
      "  g = (i + j for i in [1,2] for j in [3] if j)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::GeneratorExpr);
  const auto* ge = static_cast<const ast::GeneratorExpr*>(asg->value.get());
  ASSERT_EQ(ge->fors.size(), 2u);
  EXPECT_EQ(ge->fors[1].ifs.size(), 1u);
}

TEST(ParserCompGen, SetCompWithGuard) {
  const char* src =
      "def main() -> int:\n"
      "  s = {x for x in [1,2] if x}\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::SetComp);
  const auto* sc = static_cast<const ast::SetComp*>(asg->value.get());
  ASSERT_EQ(sc->fors.size(), 1u);
  ASSERT_EQ(sc->fors[0].ifs.size(), 1u);
}

