/***
 * Name: test_codegen_debug_di_dir_line
 * Purpose: Verify DIFile directory/filename and DISubprogram line/scopeLine reflect source locations.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

TEST(CodegenDebug, DIFileDirAndSubprogramLines) {
  // Provide a nested path to exercise directory extraction; function def at line 1
  const char* src =
      "def foo() -> int:\n"
      "  return 0\n";
  lex::Lexer L; L.pushString(src, "nested/dir/dbg_dir_file.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  auto ir = codegen::Codegen::generateIR(*mod);
  // Check DIFile filename and directory entries
  ASSERT_NE(ir.find("!DIFile(filename: \"dbg_dir_file.py\", directory: \"nested/dir\")"), std::string::npos);
  // DISubprogram line/scopeLine should use the function token's line (1)
  ASSERT_NE(ir.find("!DISubprogram(name: \"foo\", linkageName: \"foo\", scope: !1, file: !1, line: 1, scopeLine: 1"), std::string::npos);
}

