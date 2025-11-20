/***
 * Name: test_codegen_param_debug
 * Purpose: Verify parameter debug info: DILocalVariable with arg index and dbg.declare.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcParam(const char* src) {
  lex::Lexer L; L.pushString(src, "param_dbg.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenDebug, ParamLocalVariablesHaveArgIndex) {
  const char* src =
      "def add(a: int, b: int) -> int:\n"
      "  c = a\n"
      "  return c\n";
  auto mod = parseSrcParam(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // DILocalVariable entries for params with arg index 1 and 2
  ASSERT_NE(ir.find("!DILocalVariable(name: \"a\""), std::string::npos);
  ASSERT_NE(ir.find("!DILocalVariable(name: \"b\""), std::string::npos);
  ASSERT_NE(ir.find("arg: 1"), std::string::npos);
  ASSERT_NE(ir.find("arg: 2"), std::string::npos);
  // dbg.declare calls for parameter allocas
  ASSERT_NE(ir.find("call void @llvm.dbg.declare(metadata ptr %a.addr"), std::string::npos);
  ASSERT_NE(ir.find("call void @llvm.dbg.declare(metadata ptr %b.addr"), std::string::npos);
  // Basic types and DIExpression present
  ASSERT_NE(ir.find("!DIBasicType(name: \"int\""), std::string::npos);
  ASSERT_NE(ir.find("!DIExpression()"), std::string::npos);
}

