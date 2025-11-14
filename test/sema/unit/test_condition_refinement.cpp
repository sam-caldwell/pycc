/***
 * Name: test_condition_refinement
 * Purpose: Verify Sema condition refinement via visitor catches else-branch None cases.
 */
#include "sema/Sema.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include <gtest/gtest.h>

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Sema, ElseRefineNotEqNoneFlagsMismatch) {
  const char* src = R"PY(
def f(x: str) -> str:
  if not (x == None):
    return x
  else:
    return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
  // Expect at least one return type mismatch due to else branch refined to NoneType
  bool found = false;
  for (const auto& d : diags) {
    if (d.message.find("return type mismatch") != std::string::npos) { found = true; break; }
  }
  EXPECT_TRUE(found);
}

TEST(Sema, ElseRefineNeNoneFlagsMismatch) {
  const char* src = R"PY(
def f(x: str) -> str:
  if x != None:
    return x
  else:
    return x
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
  bool found = false;
  for (const auto& d : diags) {
    if (d.message.find("return type mismatch") != std::string::npos) { found = true; break; }
  }
  EXPECT_TRUE(found);
}
