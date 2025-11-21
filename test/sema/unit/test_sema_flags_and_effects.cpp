/***
 * Name: test_sema_flags_and_effects
 * Purpose: Cover Sema function flags (yield/await) and per-statement mayRaise effect typing.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src, const char* file="sema_flags.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  return P.parseModule();
}

TEST(SemaFlags, GeneratorAndCoroutineFlagsSet) {
  const char* src = R"PY(
def gen() -> int:
  x = yield 1
  return 0

def coro() -> int:
  y = await 1
  return 0
)PY";
  auto mod = parseSrc(src);
  sema::Sema S; std::vector<sema::Diagnostic> diags;
  // check() will fail due to unsupported yield/await in this subset, but flags should still be set
  (void)S.check(*mod, diags);
  const auto& flags = S.functionFlags();
  const ast::FunctionDef* genFn = nullptr; const ast::FunctionDef* coroFn = nullptr;
  for (const auto& f : mod->functions) {
    if (f->name == "gen") genFn = f.get();
    if (f->name == "coro") coroFn = f.get();
  }
  ASSERT_NE(genFn, nullptr);
  ASSERT_NE(coroFn, nullptr);
  ASSERT_NE(flags.find(genFn), flags.end());
  ASSERT_NE(flags.find(coroFn), flags.end());
  EXPECT_TRUE(flags.at(genFn).isGenerator);
  EXPECT_TRUE(flags.at(coroFn).isCoroutine);
}

TEST(SemaEffects, MayRaiseClassification) {
  const char* src = R"PY(
def main() -> int:
  a = 1 / 2
  b = 3 + 4
  return 0
)PY";
  auto mod = parseSrc(src, "effects.py");
  sema::Sema S; std::vector<sema::Diagnostic> diags; EXPECT_TRUE(S.check(*mod, diags)) << (diags.empty()?"":diags[0].message);
  // Extract statements
  ASSERT_FALSE(mod->functions.empty());
  const auto& body = mod->functions[0]->body;
  ASSERT_GE(body.size(), 3u);
  const ast::Stmt* s0 = body[0].get();
  const ast::Stmt* s1 = body[1].get();
  const ast::Stmt* s2 = body[2].get();
  // Division may raise; addition does not; return with literal does not
  EXPECT_TRUE(S.mayRaise(s0));
  EXPECT_FALSE(S.mayRaise(s1));
  EXPECT_FALSE(S.mayRaise(s2));
}

