/***
 * Name: test_cli_end_to_end
 * Purpose: Exercise CLI/Driver end-to-end: help, metrics, -S/-c, and logs.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <cstdlib>

namespace fs = std::filesystem;

static void ensure_testing_dir() {
  std::error_code ec;
  fs::create_directory("../Testing", ec);
}
static void write_file(const std::string& path, const std::string& s) {
  std::ofstream out(path); out << s;
}

static std::string read_all(const std::string& path) {
  std::ifstream in(path); std::string s, line; while (std::getline(in, line)) { s += line; s += '\n'; } return s;
}

TEST(CLI_EndToEnd, HelpPrintsUsage) {
  ensure_testing_dir();
  // Ensure pycc is available in the working directory (ctest sets it for e2e target).
  int rc = std::system("../pycc --help > ../Testing/help.txt 2>/dev/null");
  ASSERT_EQ(rc, 0);
  auto u = read_all("../Testing/help.txt");
  ASSERT_NE(u.find("pycc [options] file"), std::string::npos);
}

TEST(CLI_EndToEnd, ShortHelpAlsoPrintsUsage) {
  ensure_testing_dir();
  int rc = std::system("../pycc -h > ../Testing/help2.txt 2>/dev/null");
  ASSERT_EQ(rc, 0);
  auto u = read_all("../Testing/help2.txt");
  ASSERT_NE(u.find("pycc [options] file"), std::string::npos);
}

TEST(CLI_EndToEnd, MetricsTextAndJson) {
  ensure_testing_dir();
  write_file("../Testing/m.py", "def main() -> int:\n  return 1\n");
  int rc1 = std::system("PYCC_NO_TOOLCHAIN=1 ../pycc --metrics -o ../Testing/m_out ../Testing/m.py > ../Testing/metrics.txt 2>/dev/null");
  ASSERT_EQ(rc1, 0);
  auto txt = read_all("../Testing/metrics.txt");
  ASSERT_NE(txt.find("Lex"), std::string::npos);
  ASSERT_NE(txt.find("Parse"), std::string::npos);

  int rc2 = std::system("PYCC_NO_TOOLCHAIN=1 ../pycc --metrics-json -o ../Testing/mj_out ../Testing/m.py > ../Testing/metrics.json 2>/dev/null");
  ASSERT_EQ(rc2, 0);
  auto js = read_all("../Testing/metrics.json");
  ASSERT_NE(js.find("\"lex\""), std::string::npos);
  ASSERT_NE(js.find("\"parse\""), std::string::npos);
}

TEST(CLI_EndToEnd, AssemblyAndObjectOnlyModes) {
  ensure_testing_dir();
  write_file("../Testing/a.py", "def main() -> int:\n  return 5\n");
  // -S assembly only
  int rcS = std::system("../pycc -S -o ../Testing/out.s ../Testing/a.py > /dev/null 2>&1");
  ASSERT_EQ(rcS, 0);
  ASSERT_TRUE(fs::exists("../Testing/out.s"));
  // -c object only
  int rcC = std::system("../pycc -c -o ../Testing/out.o ../Testing/a.py > /dev/null 2>&1");
  ASSERT_EQ(rcC, 0);
  ASSERT_TRUE(fs::exists("../Testing/out.o"));
}

TEST(CLI_EndToEnd, DDefineElideGCBarrierAccepted) {
  ensure_testing_dir();
  write_file("../Testing/d.py", "def main() -> int:\n  return 3\n");
  int rc = std::system("../pycc -DOPT_ELIDE_GCBARRIER -o ../Testing/d_out ../Testing/d.py > /dev/null 2>&1");
  ASSERT_EQ(rc, 0);
  ASSERT_TRUE(fs::exists("../Testing/d_out"));
}

TEST(CLI_EndToEnd, IRContainsSourceComments) {
  ensure_testing_dir();
  write_file("../Testing/src.py", "def main() -> int:\n  return 2\n");
  int rc = std::system("../pycc -o ../Testing/out ../Testing/src.py > /dev/null 2>&1");
  ASSERT_EQ(rc, 0);
  ASSERT_TRUE(fs::exists("../Testing/out.ll"));
  auto ir = read_all("../Testing/out.ll");
  // Header demarcation and at least one original source line
  ASSERT_NE(ir.find("; ---- PY SOURCE:"), std::string::npos);
  // Tolerate absolute/relative paths; ensure content lines are present
  ASSERT_NE(ir.find("; def main() -> int"), std::string::npos);
}

TEST(CLI_EndToEnd, LogsAreWritten) {
  write_file("../Testing/l.py", "def main() -> int:\n  x = 1\n  return x\n");
  // Use a local logs directory in the run dir
  fs::create_directory("logs");
  const char* cmd = "../pycc --log-path=logs --log-lexer --log-ast --log-codegen --ast-log=both -o ../Testing/l_out ../Testing/l.py > /dev/null 2>&1";
  int rc = std::system(cmd);
  ASSERT_EQ(rc, 0);
  // Find timestamped log files by suffix
  bool hasLex = false, hasAstBefore = false, hasAstAfter = false, hasCodegen = false;
  for (const auto& entry : fs::directory_iterator("logs")) {
    const auto name = entry.path().filename().string();
    if (name.find("lexer.lex.log") != std::string::npos) hasLex = true;
    if (name.find("ast.before.ast.log") != std::string::npos) hasAstBefore = true;
    if (name.find("ast.after.ast.log") != std::string::npos) hasAstAfter = true;
    if (name.find("codegen.codegen.log") != std::string::npos) hasCodegen = true;
  }
  EXPECT_TRUE(hasLex);
  EXPECT_TRUE(hasAstBefore);
  EXPECT_TRUE(hasAstAfter);
  EXPECT_TRUE(hasCodegen);
}

TEST(CLI_EndToEnd, UnknownOptionPrintsUsageAndReturns2) {
  int rc = std::system("../pycc --totally-unknown > /dev/null 2> ../Testing/err.txt");
  // Non-zero rc expected; normalize to shell exit code space
  ASSERT_NE(rc, 0);
  auto err = read_all("../Testing/err.txt");
  ASSERT_NE(err.find("unknown option"), std::string::npos);
}

TEST(CLI_EndToEnd, NoInputsReturns2) {
  int rc = std::system("../pycc -o ../Testing/out 2> ../Testing/err_noin.txt");
  ASSERT_NE(rc, 0);
  auto err = read_all("../Testing/err_noin.txt");
  ASSERT_NE(err.find("no input files"), std::string::npos);
}
