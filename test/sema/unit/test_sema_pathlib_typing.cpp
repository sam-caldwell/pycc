/***
 * Name: test_sema_pathlib_typing
 * Purpose: Sema typing/arity checks for pathlib calls: accept valid, reject invalid.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_pathlib.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaPathlib, AcceptsCommonCalls) {
  const char* ok = R"PY(
import pathlib
def main() -> int:
  a = pathlib.cwd()
  b = pathlib.home()
  c = pathlib.join("a","b")
  d = pathlib.parent("a/b/c")
  e = pathlib.basename("a/b.txt")
  f = pathlib.suffix("a/b.txt")
  g = pathlib.stem("a/b.txt")
  h = pathlib.with_name("a/b.txt", "c.txt")
  i = pathlib.with_suffix("a/b.txt", ".log")
  j = pathlib.as_posix("a/b")
  k = pathlib.as_uri("/tmp")
  l = pathlib.resolve(".")
  m = pathlib.absolute(".")
  n = pathlib.parts("a/b/c")
  o = pathlib.exists("/not-real")
  p = pathlib.is_file("/not-real")
  q = pathlib.is_dir("/not-real")
  r = pathlib.mkdir("x", 511, 1, 1)
  s = pathlib.rmdir("x")
  t = pathlib.unlink("/not-real")
  u = pathlib.rename("a","b")
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
}

TEST(SemaPathlib, RejectsInvalidArgsAndArity) {
  const char* bad1 = R"PY(
import pathlib
def main() -> int:
  a = pathlib.cwd(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad1));

  const char* bad2 = R"PY(
import pathlib
def main() -> int:
  a = pathlib.join(1, "b")
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad2));

  const char* bad3 = R"PY(
import pathlib
def main() -> int:
  a = pathlib.mkdir("x", "bad")
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad3));

  const char* bad4 = R"PY(
import pathlib
def main() -> int:
  a = pathlib.exists(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad4));
}
