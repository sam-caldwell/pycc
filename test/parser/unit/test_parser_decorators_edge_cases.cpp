/***
 * Name: test_parser_decorators_edge_cases
 * Purpose: Exercise dotted and call decorators, and malformed decorator recovery.
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

TEST(ParserDecoratorsEdges, DecoratorDottedAndCall) {
  const char* src =
      "@a.b.c\n"
      "@decor(1, kw=2)\n"
      "def f(x: int) -> int:\n"
      "  return x\n";
  auto mod = parseSrc(src, "deco.py");
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.decorators.size(), 2u);
}

TEST(ParserDecoratorsEdges, NestedCallDecoratorAndDeepDotted) {
  const char* src =
      "@pkg.sub.deep.decor(outer(inner(1)))\n"
      "def g(y: int) -> int:\n"
      "  return y\n";
  auto mod = parseSrc(src, "deco2.py");
  ASSERT_EQ(mod->functions.size(), 1u);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.decorators.size(), 1u);
}

TEST(ParserDecoratorsEdges, MalformedDecoratorRecoveryYieldsError) {
  const char* src =
      "@decor(\n"
      "class C:\n"
      "  pass\n";
  lex::Lexer L; L.pushString(src, "deco_bad.py");
  parse::Parser P(L);
  try {
    (void)P.parseModule();
    FAIL() << "expected parse error";
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    ASSERT_NE(msg.find("parse error"), std::string::npos);
  }
}
