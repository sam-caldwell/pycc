#include <cstdlib>
#include <iostream>
#include <string>
#include <filesystem>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

int main() {
  const char* src = "def main() -> int:\n  x = 1\n  return x\n";
  pycc::lex::Lexer L; L.pushString(src, "elide_test.py");
  pycc::parse::Parser P(L);
  auto mod = P.parseModule();
  setenv("PYCC_OPT_ELIDE_GCBARRIER", "1", 1);
  setenv("PYCC_LLVM_PASS_PLUGIN_PATH", "/nonexistent/plugin.so", 1);
  // Write outputs under top-level build/Testing to avoid polluting repo root
  std::error_code ec; std::filesystem::create_directories("build/Testing", ec);
  pycc::codegen::Codegen CG(true, false);
  pycc::codegen::EmitResult res;
  std::string err = CG.emit(*mod, "build/Testing/elide_out", false, true, res);
  std::cout << "err=" << err << "\n";
  std::cout << "llPath=" << res.llPath << " objPath=" << res.objPath << "\n";
  return err.empty() ? 0 : 1;
}
