/***
 * Name: test_sema
 * Purpose: Validate minimal sema checks (names, arity, types).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Sema, HappyPath) {
  const char* src =
      "def add(a: int, b: int) -> int:\n"
      "  return a\n"
      "def main() -> int:\n"
      "  x = add(2, 3)\n"
      "  return x\n";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty() ? "" : diags[0].message);
}

TEST(Sema, UnknownNameInReturn) {
  const char* src =
      "def main() -> int:\n"
      "  return x\n";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(Sema, ArityMismatch) {
  const char* src =
      "def add(a: int, b: int) -> int:\n"
      "  return a\n"
      "def main() -> int:\n"
      "  return add(1)\n";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}
