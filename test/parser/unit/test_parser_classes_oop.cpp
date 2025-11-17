/***
 * Name: test_parser_classes_oop
 * Purpose: Ensure class definitions (bases, decorators), methods (DefStmt with decorators),
 *          class attributes, and nested classes parse correctly.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/ClassDef.h"
#include "ast/DefStmt.h"
#include "ast/AssignStmt.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "cls_oop.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ParserClasses, DecoratorsAndBases) {
  const char* src =
      "def main() -> int:\n"
      "  @dec1\n"
      "  @dec2(3)\n"
      "  class C(A, B):\n"
      "    @mdec\n"
      "    def m(self: int) -> int:\n"
      "      return 1\n"
      "    x = 42\n"
      "  return 0\n";
  // debug tokens
  {
    lex::Lexer L; L.pushString(src, "clsdbg.py");
    auto toks = L.tokens();
    for (const auto& t : toks) {
      std::fprintf(stderr, "tok %s '%s'\n", pycc::lex::to_string(t.kind), t.text.c_str());
    }
  }
  auto mod = parseSrc(src);
  const auto& fn = *mod->functions[0];
  ASSERT_EQ(fn.body.size(), 2u);
  ASSERT_EQ(fn.body[0]->kind, ast::NodeKind::ClassDef);
  const auto* cls = static_cast<const ast::ClassDef*>(fn.body[0].get());
  ASSERT_EQ(cls->decorators.size(), 2u);
  ASSERT_EQ(cls->bases.size(), 2u);
  // Body should have method (DefStmt) and an assignment
  ASSERT_EQ(cls->body.size(), 2u);
  ASSERT_EQ(cls->body[0]->kind, ast::NodeKind::DefStmt);
  const auto* defstmt = static_cast<const ast::DefStmt*>(cls->body[0].get());
  ASSERT_TRUE(defstmt->func);
  ASSERT_EQ(defstmt->func->decorators.size(), 1u);
  ASSERT_EQ(cls->body[1]->kind, ast::NodeKind::AssignStmt);
}

TEST(ParserClasses, NestedClassParses) {
  const char* src =
      "def outer() -> int:\n"
      "  class C:\n"
      "    class D:\n"
      "      pass\n"
      "  return 0\n";
  auto mod = parseSrc(src);
  const auto& outer = *mod->functions[0];
  ASSERT_EQ(outer.body[0]->kind, ast::NodeKind::ClassDef);
  const auto* C = static_cast<const ast::ClassDef*>(outer.body[0].get());
  ASSERT_EQ(C->body[0]->kind, ast::NodeKind::ClassDef);
  const auto* D = static_cast<const ast::ClassDef*>(C->body[0].get());
  ASSERT_TRUE(D != nullptr);
}
