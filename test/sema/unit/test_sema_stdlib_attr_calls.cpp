/***
 * Name: test_sema_stdlib_attr_calls
 * Purpose: Exercise stdlib attribute call typing for math/sys/subprocess in sema.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "stdlib_attr.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaStdlibAttrs, MathUnaryBinaryOkAndReject) {
  const char* src_ok = R"PY(
def f() -> int:
  a = math.sqrt(4)
  b = math.floor(3.5)
  c = math.pow(2, 3)
  return 0
)PY";
  auto mod1 = parseSrc(src_ok);
  sema::Sema S1; std::vector<sema::Diagnostic> diags1;
  EXPECT_TRUE(S1.check(*mod1, diags1)) << (diags1.empty()?"":diags1[0].message);

  const char* src_bad = R"PY(
def g() -> int:
  a = math.sqrt('x')
  return 0
)PY";
  auto mod2 = parseSrc(src_bad);
  sema::Sema S2; std::vector<sema::Diagnostic> diags2;
  EXPECT_FALSE(S2.check(*mod2, diags2));
}

TEST(SemaStdlibAttrs, SysExitAndProps) {
  const char* src = R"PY(
def f() -> int:
  sys.exit(1)
  p = sys.platform()
  v = sys.version()
  m = sys.maxsize()
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);

  const char* bad = R"PY(
def g() -> int:
  sys.exit('oops')
  return 0
)PY";
  auto modb = parseSrc(bad);
  std::vector<sema::Diagnostic> diagsb; EXPECT_FALSE(S.check(*modb, diagsb));
}

TEST(SemaStdlibAttrs, SubprocessRunTyping) {
  const char* ok = R"PY(
def f() -> int:
  rc = subprocess.run('echo hi')
  return 0
)PY";
  auto mod = parseSrc(ok);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);

  const char* bad = R"PY(
def g() -> int:
  rc = subprocess.run(1)
  return 0
)PY";
  auto modb = parseSrc(bad);
  std::vector<sema::Diagnostic> diagsb; EXPECT_FALSE(S.check(*modb, diagsb));
}

