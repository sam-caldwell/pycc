/***
 * Name: test_control_flow_semantics
 * Purpose: Control flow semantics to 100%: try handler shadowing; finally non-leak.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "ctrlflow.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(ControlFlow, ExceptShadowingDetected) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except Exception:
    return 1
  except ValueError:
    return 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

TEST(ControlFlow, ExceptSpecificThenGeneralOk) {
  const char* src = R"PY(
def f() -> int:
  try:
    return 0
  except ValueError:
    return 1
  except Exception:
    return 2
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
}

TEST(ControlFlow, FinallyDoesNotLeakNewBindings) {
  const char* src = R"PY(
def f() -> int:
  try:
    pass
  finally:
    z = 1
  return z
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags; 
  EXPECT_FALSE(S.check(*mod, diags));
}

