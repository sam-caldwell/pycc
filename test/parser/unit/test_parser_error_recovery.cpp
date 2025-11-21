/***
 * Name: test_parser_error_recovery
 * Purpose: Exercise delimiter-aware synchronization and aggregated error reporting.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserRecovery, DecoratorMalformedAndRecovery) {
  const char* src =
      "@(1,2\n"       // missing closing paren on decorator
      "def f() -> int:\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "rec.py");
  parse::Parser P(L);
  try {
    (void)P.parseModule();
    FAIL() << "expected parse error";
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    // Expect the message mentions parse error and file name
    ASSERT_NE(msg.find("rec.py"), std::string::npos);
  }
}

TEST(ParserRecovery, UnbalancedDelimitersSynchronize) {
  const char* src =
      "def g() -> int:\n"
      "  x = (1, 2\n"  // missing closing ')'
      "  return 1\n";
  lex::Lexer L; L.pushString(src, "rec2.py");
  parse::Parser P(L);
  try {
    (void)P.parseModule();
    FAIL() << "expected parse error";
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    ASSERT_NE(msg.find("rec2.py"), std::string::npos);
  }
}

