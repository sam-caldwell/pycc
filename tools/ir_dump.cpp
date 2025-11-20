#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: ir_dump <file.py>\n"; return 2; }
  std::ifstream in(argv[1]);
  if (!in) { std::cerr << "failed to open: " << argv[1] << "\n"; return 2; }
  std::ostringstream buf; buf << in.rdbuf();
  pycc::lex::Lexer L; L.pushString(buf.str(), argv[1]);
  // Debug: print tokens before parsing
  for (const auto& tok : L.tokens()) {
    std::cerr << tok.file << ":" << tok.line << ":" << tok.col << " "
              << pycc::lex::to_string(tok.kind) << " '" << tok.text << "'\n";
  }
  pycc::parse::Parser P(L);
  auto mod = P.parseModule();
  std::cout << pycc::codegen::Codegen::generateIR(*mod);
  return 0;
}
