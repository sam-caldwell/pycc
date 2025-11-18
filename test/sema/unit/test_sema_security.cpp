/***
 * Name: test_sema_security
 * Purpose: Ensure eval/exec are rejected at semantic analysis time.
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

TEST(SemaSecurity, RejectsEval) {
  const char* src = R"PY(
def main() -> int:
  x = eval("1+2")
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
  bool found = false;
  for (const auto& d : diags) { if (d.message.find("eval() is not allowed") != std::string::npos) { found = true; break; } }
  EXPECT_TRUE(found);
}

TEST(SemaSecurity, RejectsExec) {
  const char* src = R"PY(
def main() -> int:
  exec("print(1)")
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S;
  std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
  bool found = false;
  for (const auto& d : diags) { if (d.message.find("exec() is not allowed") != std::string::npos) { found = true; break; } }
  EXPECT_TRUE(found);
}

