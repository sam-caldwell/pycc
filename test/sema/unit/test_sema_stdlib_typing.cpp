/***
 * Name: test_sema_stdlib_typing
 * Purpose: Sema checks for io/sys/time/datetime stdlib calls: arity, types, and acceptances.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static bool semaOK(const char* src, const char* file="sema_stdlib.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  sema::Sema S; std::vector<sema::Diagnostic> diags; return S.check(*mod, diags);
}

TEST(SemaStdlib, IOAcceptsAndRejects) {
  const char* ok = R"PY(
def main() -> int:
  io.write_stdout("hi")
  io.write_stderr("err")
  s = io.read_file("/tmp/x")
  ok = io.write_file("/tmp/x", s)
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
def main() -> int:
  io.write_stdout(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

TEST(SemaStdlib, SysAcceptsAndRejects) {
  const char* ok = R"PY(
def main() -> int:
  a = sys.platform()
  b = sys.version()
  c = sys.maxsize()
  sys.exit(0)
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
def main() -> int:
  a = sys.platform(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

TEST(SemaStdlib, TimeAcceptsAndRejects) {
  const char* ok = R"PY(
def main() -> int:
  t = time.time()
  n = time.time_ns()
  m = time.monotonic()
  p = time.perf_counter()
  pr = time.process_time()
  time.sleep(1)
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
def main() -> int:
  time.sleep("x")
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

TEST(SemaStdlib, DatetimeAcceptsAndRejects) {
  const char* ok = R"PY(
def main() -> int:
  a = datetime.now()
  b = datetime.utcnow()
  c = datetime.fromtimestamp(0)
  d = datetime.utcfromtimestamp(0.0)
  return 0
)PY";
  EXPECT_TRUE(semaOK(ok));
  const char* bad = R"PY(
def main() -> int:
  a = datetime.now(1)
  return 0
)PY";
  EXPECT_FALSE(semaOK(bad));
}

