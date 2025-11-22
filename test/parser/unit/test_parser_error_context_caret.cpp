/***
 * Name: test_parser_error_context_caret
 * Purpose: Ensure Parser::formatContext prints source line and caret underline when file is available.
 */
#include <gtest/gtest.h>
#include <fstream>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserErrorContext, CaretAndSourceLineShown) {
  const char* path = "demos/snap_errs.py";
  // Intentionally missing ':' after return type to trigger expect() failure
  const char* src = "def f() -> int\n  return 0\n";
  {
    std::ofstream out(path); out << src; out.flush();
  }
  lex::Lexer L; L.pushFile(path);
  parse::Parser P(L);
  try {
    (void)P.parseModule();
    FAIL() << "expected parse error";
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    std::cerr << "[caret] message:\n" << msg << "\n";
    // Contains file name and caret underline under offending token line
    ASSERT_NE(msg.find("snap_errs.py"), std::string::npos);
    ASSERT_NE(msg.find("def f() -> int"), std::string::npos);
    ASSERT_NE(msg.find("^"), std::string::npos);
  }
}
