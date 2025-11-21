/***
 * Name: test_annotations_shape_only
 * Purpose: Ensure complex annotations are shape-only (no deep semantic modeling).
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="ann_shape.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaAnnotations, DictParamShapeOnly) {
  const char* src = R"PY(
def h(d: dict[str, int]) -> int:
  return 0
def main() -> int:
  return h({"a": 1.0})
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  // Shape-only means Sema does not reject the dict element type mismatch here
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

