/***
 * Name: test_parser_classes_nested
 * Purpose: Ensure nested classes parse within function/class bodies.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/ClassDef.h"

using namespace pycc;

TEST(ParserClassesNested, NestedClassParses) {
  const char* src =
      "def outer() -> int:\n"
      "  class C:\n"
      "    class D:\n"
      "      pass\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "cls_nested.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  const auto& outer = *mod->functions[0];
  ASSERT_EQ(outer.body[0]->kind, ast::NodeKind::ClassDef);
  const auto* C = static_cast<const ast::ClassDef*>(outer.body[0].get());
  ASSERT_EQ(C->body[0]->kind, ast::NodeKind::ClassDef);
  const auto* D = static_cast<const ast::ClassDef*>(C->body[0].get());
  ASSERT_TRUE(D != nullptr);
}

