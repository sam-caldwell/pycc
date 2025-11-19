/***
 * Name: test_match_semantics
 * Purpose: Validate basic semantic analysis for match/case: literals, guards, sequences, mappings, OR, class patterns, captures.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="m_sema.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(MatchSema, LiteralMatchOk) {
  const char* src = R"PY(
def main() -> int:
  match 1:
    case 1:
      return 5
    case _:
      return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(MatchSema, GuardMustBeBool) {
  const char* src = R"PY(
def main() -> int:
  match 1:
    case 1 if 2:
      return 5
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(MatchSema, SequencePatternTypeMismatch) {
  const char* src = R"PY(
def main() -> int:
  match 5:
    case [a, b]:
      return 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(MatchSema, TuplePatternBindsOk) {
  const char* src = R"PY(
def main() -> int:
  t = (1, 2)
  match t:
    case (a, b):
      return a
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(MatchSema, MappingPatternTypeMismatch) {
  const char* src = R"PY(
def main() -> int:
  match 5:
    case {'k': v}:
      return 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(MatchSema, NameCaptureInGuardOk) {
  const char* src = R"PY(
def main() -> int:
  match 3:
    case a if a == 3:
      return a
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(MatchSema, OrLiteralCaseOk) {
  const char* src = R"PY(
def main() -> int:
  match 2:
    case 1 | 2:
      return 1
    case _:
      return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(MatchSema, ClassPatternInstanceOk) {
  const char* src = R"PY(
class C:
  def __init__(self) -> None:
    return None
def main() -> int:
  c = C()
  match c:
    case C():
      return 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

