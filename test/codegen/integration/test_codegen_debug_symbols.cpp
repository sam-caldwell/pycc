/***
 * Name: test_codegen_debug_symbols
 * Purpose: Ensure emitted IR includes debug symbols: CU, DIFile, DISubprogram, DILocation and !dbg attachments.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrc(const char* src) {
  lex::Lexer L; L.pushString(src, "dbg_test.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(CodegenDebug, IRContainsDebugMetadataAndLocations) {
  const char* src =
      "def main() -> int:\n"
      "  x = 42\n"
      "  return x\n";
  auto mod = parseSrc(src);
  auto ir = codegen::Codegen::generateIR(*mod);
  // Compile unit and file
  ASSERT_NE(ir.find("!llvm.dbg.cu = !{!0}"), std::string::npos);
  ASSERT_NE(ir.find("!DICompileUnit("), std::string::npos);
  ASSERT_NE(ir.find("!DIFile(filename: \"dbg_test.py\""), std::string::npos);
  // Function attaches a DISubprogram
  auto defPos = ir.find("define i32 @main(");
  ASSERT_NE(defPos, std::string::npos);
  ASSERT_NE(ir.find("!DISubprogram(name: \"main\""), std::string::npos);
  ASSERT_NE(ir.find("!DISubprogram(name: \"main\""), std::string::npos);
  // At least one instruction has a debug attachment and there's a DILocation
  ASSERT_NE(ir.find(", !dbg !"), std::string::npos);
  ASSERT_NE(ir.find("!DILocation(line:"), std::string::npos);
  // Variable debug info for local 'x'
  ASSERT_NE(ir.find("declare void @llvm.dbg.declare(metadata, metadata, metadata)"), std::string::npos);
  ASSERT_NE(ir.find("!DILocalVariable(name: \"x\""), std::string::npos);
  ASSERT_NE(ir.find("call void @llvm.dbg.declare(metadata ptr %x.addr"), std::string::npos);
}
