/***
 * Name: test_sema_channew_and_recv
 * Purpose: Sema checks for chan_new cap typing and chan_recv acceptance.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "chan_misc.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaChannels, ChanNewCapBoolAccepted) {
  const char* src = R"PY(
def f() -> int:
  c = chan_new(True)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(SemaChannels, ChanNewCapFloatRejected) {
  const char* src = R"PY(
def f() -> int:
  c = chan_new(1.5)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaChannels, ChanRecvAccepted) {
  const char* src = R"PY(
def f() -> int:
  c = chan_new(1)
  v = chan_recv(c)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

