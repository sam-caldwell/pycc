/***
 * Name: test_codegen_elide_gcbarrier
 * Purpose: Exercise the elide-gcbarrier pass invocation path (env-driven) in Codegen::emit.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "elide_test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenElideGCBarrier, EnvTriggersOptAttempt) {
  const char* src =
      "def main() -> int:\n"
      "  x = 1\n"
      "  return x\n";
  auto mod = parseSrc(src);
  // Set env to enable the pass and supply a bogus plugin path; Codegen should attempt 'opt' but continue on failure.
  setenv("PYCC_OPT_ELIDE_GCBARRIER", "1", 1);
  setenv("PYCC_LLVM_PASS_PLUGIN_PATH", "/nonexistent/plugin.so", 1);
  codegen::Codegen CG(/*emitLL=*/true, /*emitASM=*/false);
  codegen::EmitResult res;
  std::string err = CG.emit(*mod, "elide_out", /*assemblyOnly=*/false, /*compileOnly=*/true, res);
  // Even if 'opt' fails, emit() should succeed and produce an object
  ASSERT_TRUE(err.empty());
  ASSERT_FALSE(res.llPath.empty());
}

