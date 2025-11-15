/***
 * Name: test_print_error
 * Purpose: Validate print_error output format and caret positioning.
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <unistd.h>
#include "compiler/Compiler.h"
#include "sema/Sema.h"

static std::string readFile(const std::string& p) {
  std::ifstream in(p);
  std::string all, line;
  while (std::getline(in, line)) { all += line; all += '\n'; }
  return all;
}

TEST(PrintError, WritesHeaderLabelCaret) {
  const char* srcPath = "pe_tmp.py";
  {
    std::ofstream out(srcPath);
    out << "abc\n";
    out << "xyZ\n";
  }
  // Prepare diagnostic at line 2, col 3
  pycc::sema::Diagnostic d; d.file = srcPath; d.line = 2; d.col = 3; d.message = "oops";

  // Redirect stderr to a temp file
  const char* outPath = "pe_out.txt";
  int saved = dup(2);
  FILE* fp = std::fopen(outPath, "w");
  ASSERT_NE(fp, nullptr);
  int fd = fileno(fp);
  ASSERT_GE(fd, 0);
  dup2(fd, 2);

  pycc::Compiler::print_error(d, /*color=*/false, /*context=*/2);

  // Restore stderr
  fflush(stderr);
  dup2(saved, 2);
  close(saved);
  std::fclose(fp);

  auto out = readFile(outPath);
  // Expect header, label, message
  ASSERT_NE(out.find("pe_tmp.py:2:3: "), std::string::npos);
  ASSERT_NE(out.find("error: oops"), std::string::npos);
  // Expect caret on separate line
  ASSERT_NE(out.find("^\n"), std::string::npos);
  ASSERT_NE(out.find("(context lines: 2)"), std::string::npos);
}

TEST(PrintError, ColorAddsAnsiSequences) {
  const char* srcPath = "pe_tmp2.py";
  { std::ofstream out(srcPath); out << "x\n"; }
  pycc::sema::Diagnostic d; d.file = srcPath; d.line = 1; d.col = 1; d.message = "oops";
  const char* outPath = "pe_out2.txt";
  int saved = dup(2);
  FILE* fp = std::fopen(outPath, "w");
  ASSERT_NE(fp, nullptr);
  int fd = fileno(fp);
  ASSERT_GE(fd, 0);
  dup2(fd, 2);
  pycc::Compiler::print_error(d, /*color=*/true, /*context=*/0);
  fflush(stderr);
  dup2(saved, 2);
  close(saved);
  std::fclose(fp);
  auto out = readFile(outPath);
  // Look for ESC[31m (red) and ESC[0m (reset) sequences around label
  ASSERT_NE(out.find("\x1b[31merror:"), std::string::npos);
  ASSERT_NE(out.find("\x1b[0m"), std::string::npos);
}

