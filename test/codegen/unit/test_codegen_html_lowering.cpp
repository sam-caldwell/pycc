/***
 * Name: test_codegen_html_lowering
 * Purpose: Verify lowering of html.escape/unescape.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="htmlm.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenHtml, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = html.escape("<&>")
  b = html.escape("'\"", 1)
  c = html.unescape("&amp;&lt;&gt;&quot;&#x27;")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_html_escape(ptr, i32)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_html_unescape(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_html_escape(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_html_unescape(ptr"), std::string::npos);
}

