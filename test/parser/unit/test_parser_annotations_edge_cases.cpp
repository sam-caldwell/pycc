/***
 * Name: test_parser_annotations_edge_cases
 * Purpose: Exercise advanced annotation shapes and edge cases.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* name) {
  lex::Lexer L; L.pushString(src, name);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserAnnotationsEdges, ParenthesizedGenericsInReturnAndParam) {
  const char* src =
      "def f(x: (list[int] | dict[str,int])) -> (tuple[int, str] | int):\n"
      "  return 0\n";
  auto mod = parseSrc(src, "ann.py");
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 1u);
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::List); // first inside param parens
  EXPECT_EQ(fn.returnType, ast::TypeKind::Tuple);    // first inside return parens
}

TEST(ParserAnnotationsEdges, AnnotatedAssignmentWithoutRHSAccepted) {
  const char* src =
      "def g() -> int:\n"
      "  x: list[int]\n"
      "  return 0\n";
  auto mod = parseSrc(src, "ann2.py");
  const auto& fn = *mod->functions[0];
  ASSERT_GE(fn.body.size(), 1u);
  // First stmt is an ExprStmt(Name) with type recorded
  const auto* es = dynamic_cast<const ast::ExprStmt*>(fn.body[0].get());
  ASSERT_NE(es, nullptr);
  const auto* nm = dynamic_cast<const ast::Name*>(es->value.get());
  ASSERT_NE(nm, nullptr);
  ASSERT_TRUE(nm->type().has_value());
  EXPECT_EQ(nm->type().value(), ast::TypeKind::List);
}

TEST(ParserAnnotationsEdges, InvalidAnnotationTokenIsShapeOnly) {
  const char* src =
      "def h() -> int:\n"
      "  x: 123 = 1\n"
      "  return 0\n";
  auto mod = parseSrc(src, "ann_bad.py");
  const auto& fn = *mod->functions[0];
  // Parsed as assignment, ignoring invalid type token at parser level (legality in Sema)
  const auto* asg = dynamic_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_NE(asg, nullptr);
  ASSERT_EQ(asg->targets.size(), 1u);
  const auto* nm = dynamic_cast<const ast::Name*>(asg->targets[0].get());
  ASSERT_NE(nm, nullptr);
  // No type recorded due to invalid token
  EXPECT_FALSE(nm->type().has_value());
}
