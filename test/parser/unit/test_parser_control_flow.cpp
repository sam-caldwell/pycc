/***
 * Name: test_parser_control_flow
 * Purpose: Ensure control-flow statements parse with full coverage: if/elif/else, while/else, for/else,
 *          break/continue/pass, with/async with items+as, and async for shape.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/IfStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ForStmt.h"
#include "ast/WithStmt.h"
#include "ast/WithItem.h"
#include "ast/PassStmt.h"
#include "ast/BreakStmt.h"
#include "ast/ContinueStmt.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "ctrl.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserControlFlow, IfElifElse) {
  const char* src =
      "def main() -> int:\n"
      "  if 1:\n"
      "    pass\n"
      "  elif 2:\n"
      "    pass\n"
      "  else:\n"
      "    pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::IfStmt);
  const auto* iff = static_cast<const ast::IfStmt*>(fn.body[0].get());
  ASSERT_EQ(iff->thenBody.size(), 1u);
  ASSERT_EQ(iff->elseBody.size(), 1u);
  ASSERT_EQ(iff->thenBody[0]->kind, ast::NodeKind::PassStmt);
  // elif is represented as nested IfStmt in elseBody[0]
  ASSERT_EQ(iff->elseBody[0]->kind, ast::NodeKind::IfStmt);
  const auto* elifNode = static_cast<const ast::IfStmt*>(iff->elseBody[0].get());
  ASSERT_EQ(elifNode->thenBody.size(), 1u);
}

TEST(ParserControlFlow, WhileElseBreakContinue) {
  const char* src =
      "def main() -> int:\n"
      "  while 1:\n"
      "    break\n"
      "  else:\n"
      "    continue\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::WhileStmt);
  const auto* ws = static_cast<const ast::WhileStmt*>(fn.body[0].get());
  ASSERT_EQ(ws->thenBody.size(), 1u);
  ASSERT_EQ(ws->elseBody.size(), 1u);
  ASSERT_EQ(ws->thenBody[0]->kind, ast::NodeKind::BreakStmt);
  ASSERT_EQ(ws->elseBody[0]->kind, ast::NodeKind::ContinueStmt);
}

TEST(ParserControlFlow, ForElseDestructure) {
  const char* src =
      "def main() -> int:\n"
      "  for a, b in [1,2]:\n"
      "    pass\n"
      "  else:\n"
      "    pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ForStmt);
  const auto* fs = static_cast<const ast::ForStmt*>(fn.body[0].get());
  ASSERT_EQ(fs->thenBody.size(), 1u);
  ASSERT_EQ(fs->elseBody.size(), 1u);
  ASSERT_EQ(fs->thenBody[0]->kind, ast::NodeKind::PassStmt);
  ASSERT_EQ(fs->elseBody[0]->kind, ast::NodeKind::PassStmt);
}

TEST(ParserControlFlow, WithAndAsyncWith) {
  const char* src =
      "def main() -> int:\n"
      "  with ctx as x, ctx2 as y:\n"
      "    pass\n"
      "  async with ctx3 as z:\n"
      "    pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::WithStmt);
  ASSERT_EQ(fn.body[1]->kind, ast::NodeKind::WithStmt);
  const auto* w1 = static_cast<const ast::WithStmt*>(fn.body[0].get());
  const auto* w2 = static_cast<const ast::WithStmt*>(fn.body[1].get());
  ASSERT_EQ(w1->items.size(), 2u);
  ASSERT_EQ(w2->items.size(), 1u);
  ASSERT_EQ(w1->body.size(), 1u);
  ASSERT_EQ(w2->body.size(), 1u);
  ASSERT_EQ(w1->body[0]->kind, ast::NodeKind::PassStmt);
}

TEST(ParserControlFlow, AsyncForShape) {
  const char* src =
      "def main() -> int:\n"
      "  async for a in [1]:\n"
      "    pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ForStmt);
}

