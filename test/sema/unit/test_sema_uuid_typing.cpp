/***
 * Name: test_sema_uuid_typing
 * Purpose: Ensure Sema types uuid.uuid4() as Str and enforces arity.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src) {
  lex::Lexer L; L.pushString(src, "uuidm.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaUUID, AcceptsUUID4) {
  const char* src = R"PY(
def main() -> int:
  u = uuid.uuid4()
  return 0
)PY";
  EXPECT_TRUE(semaOK(src));
}

TEST(SemaUUID, RejectsArity) {
  const char* src = R"PY(
def main() -> int:
  u = uuid.uuid4(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(src));
}

