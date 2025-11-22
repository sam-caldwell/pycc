/***
 * Name: test_codegen_module_ctors_sorted
 * Purpose: Verify module files are sorted lexicographically for init emission.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

TEST(CodegenModuleCtors, FilesSortedLexicographically) {
  lex::Lexer L;
  // Intentionally push in reverse to ensure sort happens
  L.pushString("def b() -> int:\n  return 0\n", "b.py");
  L.pushString("def a() -> int:\n  return 0\n", "a.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  auto ir = codegen::Codegen::generateIR(*mod);
  // Comments before each define indicate file order; ensure a.py comes before b.py
  const auto posa = ir.find("; module_init: a.py");
  const auto posb = ir.find("; module_init: b.py");
  ASSERT_NE(posa, std::string::npos);
  ASSERT_NE(posb, std::string::npos);
  ASSERT_LT(posa, posb);
}

