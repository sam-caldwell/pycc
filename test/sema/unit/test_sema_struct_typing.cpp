/***
 * Name: test_sema_struct_typing
 * Purpose: Ensure Sema types struct.pack/unpack/calcsize and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_struct(const char* src) {
  lex::Lexer L; L.pushString(src, "st.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaStruct, Accepts) {
  const char* src = R"PY(
def main() -> int:
  b = struct.pack('<i', [1])
  l = struct.unpack('<i', b)
  n = struct.calcsize('<i')
  return 0
)PY";
  EXPECT_TRUE(semaOK_struct(src));
}

TEST(SemaStruct, Rejects) {
  const char* src1 = R"PY(
def main() -> int:
  b = struct.pack(1, [1])
  return 0
)PY";
  EXPECT_FALSE(semaOK_struct(src1));
  const char* src2 = R"PY(
def main() -> int:
  b = struct.pack('<i', 1)
  return 0
)PY";
  EXPECT_FALSE(semaOK_struct(src2));
  const char* src3 = R"PY(
def main() -> int:
  l = struct.unpack('<i', 'not-bytes')
  return 0
)PY";
  EXPECT_FALSE(semaOK_struct(src3));
}

