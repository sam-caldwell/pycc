/***
 * Name: test_sema_subprocess_calls
 * Purpose: Ensure Sema accepts subprocess.* with string arg and rejects invalid types/arity.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "sp.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaSubprocess, AcceptsStringArgs) {
  const char* src = R"PY(
def main() -> int:
  a = subprocess.run("true")
  return 0
)PY";
  lex::Lexer L; L.pushString(src, "sp_ok.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; bool ok = S.check(*mod, diags);
  if (!ok) {
    for (const auto& d : diags) {
      std::cerr << "[sema subprocess diag] " << d.file << ":" << d.line << ":" << d.col << " " << d.message << "\n";
    }
  }
  ASSERT_TRUE(ok);
}

TEST(SemaSubprocess, RejectsNonStringOrArity) {
  const char* src1 = R"PY(
def main() -> int:
  a = subprocess.run(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src1));
  const char* src2 = R"PY(
def main() -> int:
  a = subprocess.run("true", "extra")
  return 0
)PY";
  EXPECT_FALSE(semaOK(src2));
}
