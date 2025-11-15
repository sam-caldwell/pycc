/***
 * Name: test_dce
 * Purpose: Verify dead code elimination prunes statements after returns at block and function scopes.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/DCE.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcDCE(const char* src) {
  lex::Lexer L; L.pushString(src, "dce.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(DCE, PrunesAfterReturnInFunction) {
  const char* src =
      "def main() -> int:\n"
      "  x = 1\n"
      "  return x\n"
      "  x = 2\n"
      "  return 3\n";
  auto mod = parseSrcDCE(src);
  opt::DCE dce;
  const auto removed = dce.run(*mod);
  EXPECT_GE(removed, 2u);
  const auto& fn = *mod->functions[0];
  // Body should contain only assignment + first return
  ASSERT_EQ(fn.body.size(), 2u);
  EXPECT_EQ(fn.body[0]->kind, ast::NodeKind::AssignStmt);
  EXPECT_EQ(fn.body[1]->kind, ast::NodeKind::ReturnStmt);
}

TEST(DCE, PrunesInsideIfAndElseBlocks) {
  const char* src =
      "def main() -> int:\n"
      "  if True:\n"
      "    x = 1\n"
      "    return 1\n"
      "    x = 2\n"
      "  else:\n"
      "    return 2\n"
      "    x = 3\n"
      "  return 4\n";
  auto mod = parseSrcDCE(src);
  opt::DCE dce;
  const auto removed = dce.run(*mod);
  EXPECT_GE(removed, 2u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 2u);
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::IfStmt);
  const auto* iff = static_cast<const ast::IfStmt*>(fn.body[0].get());
  // Then: keep x = 1 and return 1
  ASSERT_EQ(iff->thenBody.size(), 2u);
  EXPECT_EQ(iff->thenBody[0]->kind, ast::NodeKind::AssignStmt);
  EXPECT_EQ(iff->thenBody[1]->kind, ast::NodeKind::ReturnStmt);
  // Else: keep only return 2
  ASSERT_EQ(iff->elseBody.size(), 1u);
  EXPECT_EQ(iff->elseBody[0]->kind, ast::NodeKind::ReturnStmt);
}

