/***
 * Name: test_parser_match_multi
 * Purpose: Verify match statement with multiple cases and patterns parses.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "match.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserMatch, MultipleCases) {
  const char* src =
      "def f(x: int) -> int:\n"
      "  match x:\n"
      "    case 1:\n"
      "      return 1\n"
      "    case 2 | 3:\n"
      "      return 2\n"
      "    case _:\n"
      "      return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::MatchStmt);
}

