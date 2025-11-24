/***
 * Name: test_sema_channel_payload_positives
 * Purpose: Positive sema tests for channel payload typing (immutable-only accepted).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "chan_ok.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaChannels, SendImmutablePayloadsAccepted) {
  const char* src = R"PY(
def main() -> int:
  c = chan_new(1)
  chan_send(c, 1)
  chan_send(c, 1.0)
  chan_send(c, True)
  chan_send(c, 's')
  chan_send(c, b'xy')
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

