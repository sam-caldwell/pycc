/***
 * Name: test_parser_type_hints_extended
 * Purpose: Extend typing coverage: Optional[T], Union[T1,T2], union pipe in params/ann assigns, generics with union.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "thex.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserTypeHintsExt, OptionalAndUnionGenericsParams) {
  const char* src =
      "def f(a: Optional[int], b: Union[int, str]) -> int:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 2u);
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::Optional);
  EXPECT_EQ(fn.params[1].type, ast::TypeKind::Union);
}

TEST(ParserTypeHintsExt, ParamUnionPipeAccepted) {
  const char* src =
      "def g(a: int | None) -> int:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 1u);
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::Int);
}

TEST(ParserTypeHintsExt, AnnotatedAssignUnionAndGenerics) {
  const char* src =
      "def main() -> int:\n"
      "  x: int | None\n"
      "  y: list[int] | None = []\n"
      "  z: Union[int, str] = 0\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  // x: int | None  (ExprStmt with Name type Int)
  {
    const auto* es = static_cast<const ast::ExprStmt*>(fn.body[0].get());
    const auto* nm = static_cast<const ast::Name*>(es->value.get());
    ASSERT_TRUE(nm->type().has_value());
    EXPECT_EQ(nm->type().value(), ast::TypeKind::Int);
  }
  // y: list[int] | None = []
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[1].get());
    const auto* nm = static_cast<const ast::Name*>(asg->targets[0].get());
    ASSERT_TRUE(nm->type().has_value());
    EXPECT_EQ(nm->type().value(), ast::TypeKind::List);
  }
  // z: Union[int, str]
  {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[2].get());
    const auto* nm = static_cast<const ast::Name*>(asg->targets[0].get());
    ASSERT_TRUE(nm->type().has_value());
    EXPECT_EQ(nm->type().value(), ast::TypeKind::Union);
  }
}

