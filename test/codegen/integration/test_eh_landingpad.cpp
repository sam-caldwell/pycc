/***
 * Name: test_eh_landingpad
 * Purpose: Ensure functions have personality and try/raise uses landingpad with invoke.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "eh.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenIR, TryRaiseEmitsLandingpadAndPersonality) {
  const char* src =
      "def main() -> int:\n"
      "  try:\n"
      "    raise Exception(\"x\")\n"
      "  except Exception as e:\n"
      "    return 0\n"
      "  return 1\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  ASSERT_NE(ir.find("personality ptr @__gxx_personality_v0"), std::string::npos);
  ASSERT_NE(ir.find("landingpad { ptr, i32 } cleanup"), std::string::npos);
  ASSERT_NE(ir.find("invoke void @pycc_rt_raise(ptr"), std::string::npos);
}

