/***
 * Name: test_parser_class_decorators
 * Purpose: Verify decorators on class methods attach to FunctionDef.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

TEST(ParserClassDecorators, MethodDecoratorAttached) {
  const char* src =
      "@top\n"
      "def ignored() -> int:\n"
      "  return 0\n"
      "class C:\n"
      "  @dec\n"
      "  def m(self: int) -> int:\n"
      "    return 1\n";
  lex::Lexer L; L.pushString(src, "cls.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  ASSERT_TRUE(mod);
  // We don't store classes at module-level yet; this just ensures parse success.
  // Reparse the class body through Parser API: we rely on parseClass() via parseModule() and our side-effect only.
  // Instead, embed the class in a function so we can inspect body; simpler approach:
  const char* src2 =
      "def main() -> int:\n"
      "  class D:\n"
      "    @dec\n"
      "    def m(self: int) -> int:\n"
      "      return 2\n"
      "  return 0\n";
  lex::Lexer L2; L2.pushString(src2, "cls2.py");
  parse::Parser P2(L2);
  auto mod2 = P2.parseModule();
  ASSERT_TRUE(mod2);
  const auto& fn = *mod2->functions[0];
  // First stmt in function is a ClassDef
  ASSERT_EQ(fn.body.size(), 2u);
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ClassDef);
  const auto* cls = static_cast<const ast::ClassDef*>(fn.body[0].get());
  // Class body first stmt should be DefStmt wrapping a FunctionDef
  ASSERT_FALSE(cls->body.empty());
  ASSERT_EQ(cls->body[0]->kind, ast::NodeKind::DefStmt);
  const auto* defstmt = static_cast<const ast::DefStmt*>(cls->body[0].get());
  ASSERT_TRUE(defstmt->func);
  ASSERT_EQ(defstmt->func->decorators.size(), 1u);
}

