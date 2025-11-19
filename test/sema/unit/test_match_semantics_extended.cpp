/***
 * Name: test_match_semantics_extended
 * Purpose: Extend match semantics coverage: starred sequence, mapping rest, class mismatch, and capture non-leak.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="m_sema_ext.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(MatchSemaExt, SequenceStarBindsListRest) {
  const char* src = R"PY(
def main() -> int:
  xs = [1,2,3]
  match xs:
    case [a, *rest]:
      return len(rest)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(MatchSemaExt, MappingRestBindsDict) {
  const char* src = R"PY(
def main() -> int:
  d = {'k': 1, 'q': 2}
  match d:
    case {'k': v, **rest}:
      return len(rest)
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(MatchSemaExt, ClassPatternWrongInstanceFails) {
  const char* src = R"PY(
class C:
  def __init__(self) -> None:
    return None
class D:
  def __init__(self) -> None:
    return None
def main() -> int:
  d = D()
  match d:
    case C():
      return 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

TEST(MatchSemaExt, CaptureDoesNotLeakOutsideCase) {
  const char* src = R"PY(
def main() -> int:
  match 1:
    case a:
      pass
  return a
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_FALSE(S.check(*mod, diags));
}

