/***
 * Name: test_parser_func_def_call
 * Purpose: Ensure function defs and calls (defaults, kw-only, splats, annotations, decorators, async def) parse correctly.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/FunctionDef.h"
#include "ast/DefStmt.h"
#include "ast/Call.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "fdc.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserFuncDef, KwOnlyBareStarAndKwVarArg) {
  const char* src =
      "def f(a, *, b: int=2, **kw) -> int:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.params.size(), 3u);
  // a is normal
  EXPECT_EQ(fn.params[0].name, std::string("a"));
  EXPECT_FALSE(fn.params[0].isKwOnly);
  // b is kw-only with default and annotation
  EXPECT_EQ(fn.params[1].name, std::string("b"));
  EXPECT_TRUE(fn.params[1].isKwOnly);
  EXPECT_TRUE(static_cast<bool>(fn.params[1].defaultValue));
  EXPECT_EQ(fn.params[1].type, ast::TypeKind::Int);
  // **kw present
  EXPECT_TRUE(fn.params[2].isKwVarArg);
}

TEST(ParserFuncDef, AsyncDefAccepted) {
  const char* src =
      "async def g(x: int) -> int:\n"
      "  return x\n";
  auto mod = parseSrc(src);
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& fn = *mod->functions[0];
  EXPECT_EQ(fn.name, std::string("g"));
}

TEST(ParserFuncDef, TopLevelDecoratorsAttached) {
  const char* src =
      "@dec1\n"
      "@dec2(3)\n"
      "def h() -> int:\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.decorators.size(), 2u);
}

TEST(ParserCall, PositionalKeywordStarargsKwstarargs) {
  const char* src =
      "def main() -> int:\n"
      "  z = f(1, *xs, y=2, **kw)\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[0].get());
  ASSERT_EQ(asg->value->kind, ast::NodeKind::Call);
  const auto* call = static_cast<const ast::Call*>(asg->value.get());
  ASSERT_EQ(call->args.size(), 1u);
  ASSERT_EQ(call->starArgs.size(), 1u);
  ASSERT_EQ(call->keywords.size(), 1u);
  ASSERT_EQ(call->kwStarArgs.size(), 1u);
  EXPECT_EQ(call->keywords[0].name, std::string("y"));
}
