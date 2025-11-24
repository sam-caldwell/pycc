/***
 * Name: test_loop_unroll
 * Purpose: Validate LoopUnroll transforms for-range loops with small constant trips
 *          into unrolled sequences and preserves else semantics. Also ensure
 *          it skips complex/unsafe cases.
 */
#include <gtest/gtest.h>
#include "optimizer/LoopUnroll.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="unroll.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L); return P.parseModule();
}

static size_t countKind(const ast::FunctionDef& fn, ast::NodeKind k) {
  size_t n = 0;
  for (const auto& s : fn.body) if (s && s->kind == k) ++n;
  return n;
}

TEST(LoopUnroll, UnrollsRangeSingleArgSmallBody) {
  const char* src = R"PY(
def f() -> int:
  x = 0
  for i in range(3):
    x = x + 1
  return x
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  ASSERT_EQ(n, 1u);
  ASSERT_FALSE(mod->functions.empty());
  const auto& fn = *mod->functions[0];
  // No loops remain at top-level
  EXPECT_EQ(countKind(fn, ast::NodeKind::ForStmt), 0u);
  // Expect three assignments to loop index 'i'
  size_t iAssigns = 0; std::vector<long long> vals;
  for (const auto& st : fn.body) {
    if (!st || st->kind != ast::NodeKind::AssignStmt) continue;
    const auto* as = static_cast<const ast::AssignStmt*>(st.get());
    if (as->target == "i" && as->value && as->value->kind == ast::NodeKind::IntLiteral) {
      ++iAssigns; vals.push_back(static_cast<const ast::IntLiteral*>(as->value.get())->value);
    }
  }
  EXPECT_EQ(iAssigns, 3u);
  ASSERT_EQ(vals.size(), 3u);
  EXPECT_EQ(vals[0], 0);
  EXPECT_EQ(vals[1], 1);
  EXPECT_EQ(vals[2], 2);
}

TEST(LoopUnroll, UnrollsRangeStartStopStep) {
  const char* src = R"PY(
def g() -> int:
  s = 0
  for i in range(1,5,2):
    s = s + i
  return s
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  ASSERT_EQ(n, 1u);
  const auto& fn = *mod->functions[0];
  EXPECT_EQ(countKind(fn, ast::NodeKind::ForStmt), 0u);
  // Expected loop indices: 1, 3
  std::vector<long long> vals;
  for (const auto& st : fn.body) {
    if (!st || st->kind != ast::NodeKind::AssignStmt) continue;
    const auto* as = static_cast<const ast::AssignStmt*>(st.get());
    if (as->target == "i" && as->value && as->value->kind == ast::NodeKind::IntLiteral) {
      vals.push_back(static_cast<const ast::IntLiteral*>(as->value.get())->value);
    }
  }
  ASSERT_EQ(vals.size(), 2u);
  EXPECT_EQ(vals[0], 1);
  EXPECT_EQ(vals[1], 3);
}

TEST(LoopUnroll, ElseRunsWhenZeroTrips) {
  const char* src = R"PY(
def h() -> int:
  x = 1
  for i in range(0):
    x = 2
  else:
    x = 42
  return x
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  // Zero-trip loops replaced entirely with else body; still counts as a transform
  ASSERT_EQ(n, 1u);
  const auto& fn = *mod->functions[0];
  EXPECT_EQ(countKind(fn, ast::NodeKind::ForStmt), 0u);
  // Find the last assignment; it should set x to 42
  long long lastVal = -1; std::string lastTarget;
  for (const auto& st : fn.body) {
    if (!st || st->kind != ast::NodeKind::AssignStmt) continue;
    const auto* as = static_cast<const ast::AssignStmt*>(st.get());
    if (as->value && as->value->kind == ast::NodeKind::IntLiteral) {
      lastVal = static_cast<const ast::IntLiteral*>(as->value.get())->value;
      lastTarget = as->target;
    }
  }
  EXPECT_EQ(lastTarget, "x");
  EXPECT_EQ(lastVal, 42);
}

TEST(LoopUnroll, SkipsLargeTripCount) {
  const char* src = R"PY(
def k() -> int:
  s = 0
  for i in range(100):
    s = s + 1
  return s
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  EXPECT_EQ(n, 0u);
  const auto& fn = *mod->functions[0];
  EXPECT_EQ(countKind(fn, ast::NodeKind::ForStmt), 1u);
}

TEST(LoopUnroll, SkipsNegativeStep) {
  const char* src = R"PY(
def m() -> int:
  s = 0
  for i in range(5, 1, -1):
    s = s + 1
  return s
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  EXPECT_EQ(n, 0u);
}

TEST(LoopUnroll, SkipsComplexBody) {
  const char* src = R"PY(
def p() -> int:
  s = 0
  for i in range(3):
    print(s)
  return s
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  EXPECT_EQ(n, 0u);
}

TEST(LoopUnroll, SkipsDestructuringTarget) {
  const char* src = R"PY(
def q() -> int:
  s = 0
  for (i, j) in range(3):
    s = s + 1
  return s
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  EXPECT_EQ(n, 0u);
}

TEST(LoopUnroll, StepZeroIgnored) {
  const char* src = R"PY(
def r() -> int:
  s = 0
  for i in range(1,5,0):
    s = s + 1
  return s
)PY";
  auto mod = parseSrc(src);
  opt::LoopUnroll un; auto n = un.run(*mod);
  EXPECT_EQ(n, 0u);
}

