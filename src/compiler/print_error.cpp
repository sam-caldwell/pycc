#include "compiler/Compiler.h"
#include "sema/Sema.h"
#include <fstream>

extern "C" long write(int, const void*, unsigned long);

namespace pycc {

static void write_str(const char* str) {
  unsigned long len = 0;
  while (str[len] != '\0') { ++len; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  (void)write(2, str, len);
}

static void write_int(int value) {
  constexpr int kBufSize = 32;
  constexpr int kBase10 = 10;
  char buf[kBufSize]; // NOLINT(cppcoreguidelines-avoid-c-arrays)
  char* const begin = &buf[0];      // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  char* cur = begin;                // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (value == 0) { write_str("0"); return; }
  if (value < 0) { (void)write(2, "-", 1); value = -value; }
  while (value > 0 && (cur - begin) < kBufSize) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const int digit = value % kBase10;
    *cur = static_cast<char>('0' + digit);      // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ++cur;                                       // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    value /= kBase10;
  }
  while (cur != begin) {                         // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    --cur;                                       // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    (void)write(2, cur, 1);
  }
}

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
