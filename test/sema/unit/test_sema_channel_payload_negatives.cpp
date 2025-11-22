/***
 * Name: test_sema_channel_payload_negatives
 * Purpose: Negative sema tests for channel payload typing (immutable-only enforcement).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "chan.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaChannels, SendListPayloadRejected) {
  const char* src = R"PY(
def main() -> int:
  c = chan_new(1)
  chan_send(c, [1,2,3])
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(SemaChannels, SendDictPayloadRejected) {
  const char* src = R"PY(
def main() -> int:
  c = chan_new(1)
  chan_send(c, {1: 2})
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_FALSE(S.check(*mod, diags));
}

