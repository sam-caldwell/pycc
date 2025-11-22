/***
 * Name: test_parser_recovery_snapshots
 * Purpose: Add recovery snapshot tests for malformed decorators and imports to validate diagnostics and recovery.
 */
#include <gtest/gtest.h>
#include <fstream>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserRecoverySnapshots, MalformedDecoratorThenValidDef) {
  const char* path = "demos/snap_deco.py";
  const char* src =
      "@decor(\n"      // malformed decorator expression (missing ')')
      "def ok() -> int:\n"
      "  return 0\n";
  { std::ofstream out(path); out << src; out.flush(); }
  lex::Lexer L; L.pushFile(path);
  parse::Parser P(L);
  try {
    auto mod = P.parseModule();
    (void)mod;
    FAIL() << "expected aggregated parse error";
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    std::cerr << "[snapshot-decorator] message:\n" << msg << "\n";
    // Message shows expectation and recovered notes; file name may be empty at EOF
    ASSERT_NE(msg.find("expected ')', got"), std::string::npos);
    ASSERT_NE(msg.find("note:"), std::string::npos);
  }
}

TEST(ParserRecoverySnapshots, ImportFromMissingIdentAfterDot) {
  const char* path = "demos/snap_import.py";
  const char* src =
      "from pkg. import x\n"  // error: missing ident after '.'
      "def main() -> int:\n"
      "  return 0\n";
  { std::ofstream out(path); out << src; out.flush(); }
  lex::Lexer L; L.pushFile(path);
  parse::Parser P(L);
  try {
    (void)P.parseModule();
    FAIL() << "expected parse error";
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    // Verify message references import context and shows caret
    ASSERT_NE(msg.find("snap_import.py"), std::string::npos);
    ASSERT_NE(msg.find("expected ident after '.' in from"), std::string::npos);
    ASSERT_NE(msg.find("^"), std::string::npos);
  }
}
