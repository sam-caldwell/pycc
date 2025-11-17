/***
 * Name: test_parser_type_hints
 * Purpose: Ensure type hints are understood by lexer/parser/ast.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "th.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserTypeHints, FunctionParamsAndReturn) {
  const char* src =
      "def f(a: int, b: list) -> dict:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 2u);
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::Int);
  EXPECT_EQ(fn.params[1].type, ast::TypeKind::List);
  EXPECT_EQ(fn.returnType, ast::TypeKind::Dict);
}

TEST(ParserTypeHints, AnnotatedAssignment) {
  const char* src =
      "def main() -> int:\n"
      "  x: float = 1.0\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->targets.size(), 1u);
  const auto* nm = static_cast<const ast::Name*>(asg->targets[0].get());
  ASSERT_TRUE(nm->type().has_value());
  EXPECT_EQ(nm->type().value(), ast::TypeKind::Float);
}

TEST(ParserTypeHints, OptionalAndUnionTokens) {
  const char* src =
      "def g(a: Optional) -> Union:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 1u);
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::Optional);
  EXPECT_EQ(fn.returnType, ast::TypeKind::Union);
}

TEST(ParserTypeHints, UnionReturnPipeAccepted) {
  const char* src =
      "def h() -> int | None:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  // We accept the syntax and record the first type for now
  EXPECT_EQ(fn.returnType, ast::TypeKind::Int);
}

TEST(ParserTypeHints, GenericsShapeParamsAndReturn) {
  const char* src =
      "def f(a: list[int], b: dict[str, int], c: tuple[int, str]) -> list[int]:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 3u);
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::List);
  EXPECT_EQ(fn.params[1].type, ast::TypeKind::Dict);
  EXPECT_EQ(fn.params[2].type, ast::TypeKind::Tuple);
  EXPECT_EQ(fn.returnType, ast::TypeKind::List);
}

TEST(ParserTypeHints, GenericsAnnotatedAssignment) {
  const char* src =
      "def main() -> int:\n"
      "  x: list[int] = []\n"
      "  y: dict[str, int] = {}\n"
      "  z: tuple[int, str] = (1, 'a')\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
    const auto* nm = static_cast<const ast::Name*>(asg->targets[0].get());
    ASSERT_TRUE(nm->type().has_value());
    EXPECT_EQ(nm->type().value(), ast::TypeKind::List);
  }
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[1].get());
    const auto* nm = static_cast<const ast::Name*>(asg->targets[0].get());
    ASSERT_TRUE(nm->type().has_value());
    EXPECT_EQ(nm->type().value(), ast::TypeKind::Dict);
  }
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[2].get());
    const auto* nm = static_cast<const ast::Name*>(asg->targets[0].get());
    ASSERT_TRUE(nm->type().has_value());
    EXPECT_EQ(nm->type().value(), ast::TypeKind::Tuple);
  }
}
