/***
 * Name: test_sema__asyncio_typing
 * Purpose: Validate typing/arity for _asyncio helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema__asyncio.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(Sema_Asyncio, AcceptsAndRejects) {
  const char* ok = R"PY(
import _asyncio
def main() -> int:
  loop = _asyncio.get_event_loop()
  fut = _asyncio.Future()
  _asyncio.future_set_result(fut, "x")
  r = _asyncio.future_result(fut)
  d = _asyncio.future_done(fut)
  _asyncio.sleep(1)
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
import _asyncio
def main() -> int:
  _asyncio.future_set_result(1, "x")
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

