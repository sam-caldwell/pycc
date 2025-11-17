/***
 * Name: test_parser_func_more
 * Purpose: Round out Function Def/Call coverage: typed varargs/kwvarargs, trailing commas in params and args,
 *          and nested def as statement.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/DefStmt.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "fmore.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserFuncMore, TypedVarArgAndKwVarArg) {
  const char* src =
      "def f(*args: int, **kw: bool) -> int:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 2u);
  EXPECT_TRUE(fn.params[0].isVarArg);
  EXPECT_EQ(fn.params[0].type, ast::TypeKind::Int);
  EXPECT_TRUE(fn.params[1].isKwVarArg);
  EXPECT_EQ(fn.params[1].type, ast::TypeKind::Bool);
}

TEST(ParserFuncMore, TrailingCommaInParamsAndArgs) {
  const char* src =
      "def g(a: int, b: int,) -> int:\n"
      "  z = g(1, 2, y=3,)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 2u);
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Call);
}

TEST(ParserFuncMore, NestedDefAsStatement) {
  const char* src =
      "def outer() -> int:\n"
      "  def inner(x: int) -> int:\n"
      "    return x\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& outer = *mod->functions[0];
  ASSERT_EQ(outer.body[0]->kind, ast::NodeKind::DefStmt);
  const auto* defstmt = static_cast<const ast::DefStmt*>(outer.body[0].get());
  ASSERT_TRUE(defstmt->func);
  EXPECT_EQ(defstmt->func->name, std::string("inner"));
  ASSERT_EQ(defstmt->func->params.size(), 1u);
}

