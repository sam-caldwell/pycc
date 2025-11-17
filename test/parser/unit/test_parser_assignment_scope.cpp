/***
 * Name: test_parser_assignment_scope
 * Purpose: Ensure assignment targets and scope-related statements are fully covered:
 *          attribute/subscript targets with Store ctx; del contexts; aug-assign on attr/subscript;
 *          annotated assignment without RHS; globals/nonlocals already covered elsewhere.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/AssignStmt.h"
#include "ast/ExprStmt.h"
#include "ast/Attribute.h"
#include "ast/Subscript.h"
#include "ast/TupleLiteral.h"
#include "ast/Name.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "ascope.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserAssignScope, StoreCtxForAttrAndSubscript) {
  const char* src =
      "def main() -> int:\n"
      "  a.b = 1\n"
      "  c[0] = 2\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
    ASSERT_EQ(asg->targets.size(), 1u);
    ASSERT_EQ(asg->targets[0]->kind, ast::NodeKind::Attribute);
    const auto* attr = static_cast<const ast::Attribute*>(asg->targets[0].get());
    EXPECT_EQ(attr->ctx, ast::ExprContext::Store);
  }
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[1].get());
    ASSERT_EQ(asg->targets.size(), 1u);
    ASSERT_EQ(asg->targets[0]->kind, ast::NodeKind::Subscript);
    const auto* sub = static_cast<const ast::Subscript*>(asg->targets[0].get());
    EXPECT_EQ(sub->ctx, ast::ExprContext::Store);
  }
}

TEST(ParserAssignScope, DelStmtTargetsCtx) {
  const char* src =
      "def main() -> int:\n"
      "  del a, b[0], c.d, (x, y)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::DelStmt);
  const auto* ds = static_cast<const ast::DelStmt*>(fn.body[0].get());
  ASSERT_EQ(ds->targets.size(), 4u);
  ASSERT_EQ(ds->targets[0]->kind, ast::NodeKind::Name);
  EXPECT_EQ(static_cast<const ast::Name*>(ds->targets[0].get())->ctx, ast::ExprContext::Del);
  ASSERT_EQ(ds->targets[1]->kind, ast::NodeKind::Subscript);
  EXPECT_EQ(static_cast<const ast::Subscript*>(ds->targets[1].get())->ctx, ast::ExprContext::Del);
  ASSERT_EQ(ds->targets[2]->kind, ast::NodeKind::Attribute);
  EXPECT_EQ(static_cast<const ast::Attribute*>(ds->targets[2].get())->ctx, ast::ExprContext::Del);
  ASSERT_EQ(ds->targets[3]->kind, ast::NodeKind::TupleLiteral);
}

TEST(ParserAssignScope, AugAssignAttrAndSubscript) {
  const char* src =
      "def main() -> int:\n"
      "  a.b += 1\n"
      "  c[0] -= 2\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::AugAssignStmt);
  ASSERT_EQ(fn.body[1]->kind, ast::NodeKind::AugAssignStmt);
}

TEST(ParserAssignScope, AnnotatedAssignmentNoRHS) {
  const char* src =
      "def main() -> int:\n"
      "  x: int\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  // Should produce an ExprStmt containing a Name with type annotation
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ExprStmt);
  const auto* es = static_cast<const ast::ExprStmt*>(fn.body[0].get());
  ASSERT_EQ(es->value->kind, ast::NodeKind::Name);
  const auto* nm = static_cast<const ast::Name*>(es->value.get());
  ASSERT_TRUE(nm->type().has_value());
  EXPECT_EQ(nm->type().value(), ast::TypeKind::Int);
}

