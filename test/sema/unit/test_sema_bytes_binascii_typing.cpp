/***
 * Name: test_sema_bytes_binascii_typing
 * Purpose: Ensure Sema types encode/decode/binascii correctly and rejects invalid usages.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK_src(const char* src) {
  lex::Lexer L; L.pushString(src, "mod.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaBytesBinascii, AcceptsEncodeDecode) {
  const char* src1 = R"PY(
def main() -> int:
  s = 'Hi'
  b = s.encode('utf-8')
  return 0
)PY";
  EXPECT_TRUE(semaOK_src(src1));

  const char* src2 = R"PY(
def main() -> int:
  b = b'Hi'
  s = b.decode('ascii')
  return 0
)PY";
  EXPECT_TRUE(semaOK_src(src2));
}

TEST(SemaBytesBinascii, RejectsWrongEncodeDecodeUsage) {
  const char* encWrongBase = R"PY(
def main() -> int:
  x = 1
  y = x.encode('utf-8')
  return 0
)PY";
  EXPECT_FALSE(semaOK_src(encWrongBase));

  const char* encWrongArgs = R"PY(
def main() -> int:
  y = 'x'.encode(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK_src(encWrongArgs));

  const char* encWrongErrors = R"PY(
def main() -> int:
  y = 'x'.encode('utf-8', 1)
  return 0
)PY";
  EXPECT_FALSE(semaOK_src(encWrongErrors));

  const char* decWrongBase = R"PY(
def main() -> int:
  s = 'x'
  y = s.decode('utf-8')
  return 0
)PY";
  EXPECT_FALSE(semaOK_src(decWrongBase));

  const char* decWrongArgs = R"PY(
def main() -> int:
  b = b'Hi'
  y = b.decode(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK_src(decWrongArgs));
}

TEST(SemaBytesBinascii, BinasciiAccepts) {
  const char* hexlifyBytes = R"PY(
def main() -> int:
  import binascii
  h = binascii.hexlify(b'Hi')
  return 0
)PY";
  EXPECT_TRUE(semaOK_src(hexlifyBytes));

  const char* unhexlifyStr = R"PY(
def main() -> int:
  import binascii
  b = binascii.unhexlify('4869')
  return 0
)PY";
  EXPECT_TRUE(semaOK_src(unhexlifyStr));

  const char* unhexlifyBytes = R"PY(
def main() -> int:
  import binascii
  h = binascii.hexlify(b'Hi')
  b = binascii.unhexlify(h)
  return 0
)PY";
  EXPECT_TRUE(semaOK_src(unhexlifyBytes));
}

TEST(SemaBytesBinascii, BinasciiRejects) {
  const char* hexlifyStr = R"PY(
def main() -> int:
  import binascii
  h = binascii.hexlify('Hi')
  return 0
)PY";
  EXPECT_FALSE(semaOK_src(hexlifyStr));

  const char* unhexlifyWrong = R"PY(
def main() -> int:
  import binascii
  b = binascii.unhexlify(123)
  return 0
)PY";
  EXPECT_FALSE(semaOK_src(unhexlifyWrong));
}

