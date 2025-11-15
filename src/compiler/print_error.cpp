#include "compiler/Compiler.h"
#include "sema/Sema.h"
#include <fstream>
#include <cstring>

extern "C" long write(int, const void*, unsigned long);

namespace pycc {

static void write_str(const char* str) {
  if (str == nullptr) { return; }
  const auto len = std::strlen(str);
  (void)write(2, str, static_cast<unsigned long>(len));
}

static void write_int(int value) {
  const auto s = std::to_string(value);
  (void)write(2, s.c_str(), static_cast<unsigned long>(s.size()));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void Compiler::print_error(const sema::Diagnostic& diag, const bool color, const int context) {
  const char* red = "\033[31m";
  const char* bold = "\033[1m";
  const char* reset = "\033[0m";

  if (!diag.file.empty()) {
    if (color) { write_str(bold); }
    write_str(diag.file.c_str());
    write_str(":"); write_int(diag.line);
    write_str(":"); write_int(diag.col);
    write_str(": ");
    if (color) { write_str(reset); }
  }
  if (color) { write_str(red); write_str("error: "); write_str(reset); }
  else { write_str("error: "); }
  write_str(diag.message.c_str());
  write_str("\n");

  // Best-effort: print source line and caret under column
  if (!diag.file.empty() && diag.line > 0 && diag.col > 0) {
    std::ifstream in(diag.file);
    if (in) {
      std::string lineStr;
      int curLine = 1;
      while (curLine < diag.line && std::getline(in, lineStr)) { ++curLine; }
      if (curLine == diag.line && std::getline(in, lineStr)) {
        write_str("  "); write_str(lineStr.c_str()); write_str("\n");
        write_str("  ");
        for (int i = 1; i < diag.col; ++i) { write_str(" "); }
        write_str("^\n");
      }
    }
  }

  // Indicate context lines requested
  if (context > 0) { write_str("  (context lines: "); write_int(context); write_str(")\n"); }
}

} // namespace pycc
