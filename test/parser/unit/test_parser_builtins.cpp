/***
 * Name: test_parser_builtins
 * Purpose: Ensure a broad set of built-in calls parse as Call nodes with Name callees.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/Call.h"
#include "ast/Name.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "builtins.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserBuiltins, CommonCallsParseAsCall) {
  const char* src =
      "def main() -> int:\n"
      "  a = len([1,2])\n"
      "  b = isinstance(x, int)\n"
      "  c = int(3.2)\n"
      "  d = float(3)\n"
      "  e = bool(0)\n"
      "  f = str('x')\n"
      "  g = list((1,2))\n"
      "  h = dict()\n"
      "  i = tuple([1,2])\n"
      "  j = range(1,5,2)\n"
      "  k = sum([1,2,3])\n"
      "  m = map(int, ['1','2'])\n"
      "  n = print('hi')\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  const char* names[] = {"len","isinstance","int","float","bool","str","list","dict","tuple","range","sum","map","print"};
  for (int idx = 0; idx < 13; ++idx) {
    const auto* asg = static_cast<const ast::AssignStmt*>(fn.body[idx].get());
    ASSERT_EQ(asg->value->kind, ast::NodeKind::Call) << idx;
    const auto* call = static_cast<const ast::Call*>(asg->value.get());
    ASSERT_TRUE(call->callee);
    ASSERT_EQ(call->callee->kind, ast::NodeKind::Name);
    const auto* nm = static_cast<const ast::Name*>(call->callee.get());
    EXPECT_EQ(nm->id, std::string(names[idx])) << idx;
  }
}

