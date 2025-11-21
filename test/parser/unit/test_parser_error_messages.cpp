/***
 * Name: test_parser_error_messages
 * Purpose: Check wording and aggregation of parser error messages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserErrorMessages, AggregatedWithNotesAndFilename) {
  const char* src =
      "@(1,2\n"            // malformed decorator (missing ')')
      "def f( -> int:\n"   // malformed signature
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "errs.py");
  parse::Parser P(L);
  try {
    (void)P.parseModule();
    FAIL() << "expected parse error";
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    // Includes file name and parse error wording
    ASSERT_NE(msg.find("errs.py"), std::string::npos);
    ASSERT_NE(msg.find("parse error"), std::string::npos);
    // Contains at least one note
    ASSERT_NE(msg.find("note:"), std::string::npos);
  }
}

