/***
 * Name: test_parser_exceptions
 * Purpose: Ensure try/except[/else]/finally, raise with cause, and assert with message parse correctly.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/TryStmt.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "exc.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserExceptions, TryExceptElseFinallyAndRaiseFrom) {
  const char* src =
      "def main() -> int:\n"
      "  try:\n"
      "    a = 1\n"
      "  except ValueError as e:\n"
      "    a = 2\n"
      "  else:\n"
      "    a = 3\n"
      "  finally:\n"
      "    a = 4\n"
      "  raise RuntimeError('x') from e\n"
      "  assert a, 'bad'\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::TryStmt);
  const auto* ts = static_cast<const ast::TryStmt*>(fn.body[0].get());
  ASSERT_EQ(ts->handlers.size(), 1u);
  ASSERT_EQ(ts->orelse.size(), 1u);
  ASSERT_EQ(ts->finalbody.size(), 1u);
  ASSERT_EQ(fn.body[1]->kind, ast::NodeKind::RaiseStmt);
  ASSERT_EQ(fn.body[2]->kind, ast::NodeKind::AssertStmt);
}

TEST(ParserExceptions, TryFinallyOnlyAndBareExcept) {
  const char* src =
      "def main() -> int:\n"
      "  try:\n"
      "    pass\n"
      "  finally:\n"
      "    pass\n"
      "  try:\n"
      "    pass\n"
      "  except:\n"
      "    pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::TryStmt);
  const auto* ts1 = static_cast<const ast::TryStmt*>(fn.body[0].get());
  ASSERT_TRUE(ts1->handlers.empty());
  ASSERT_FALSE(ts1->finalbody.empty());
  const auto* ts2 = static_cast<const ast::TryStmt*>(fn.body[1].get());
  ASSERT_EQ(ts2->handlers.size(), 1u);
}

