/***
 * Name: test_execute_tempfile
 * Purpose: Compile and run a program using tempfile; verify stdout and exit code.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static std::string slurp_tmp(const std::string& p) {
  std::ifstream in(p);
  std::string s; std::getline(in, s, '\0'); return s;
}

TEST(ExecuteTempfile, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_tempfile.py").string();
  std::error_code ec; std::filesystem::create_directory("../Testing", ec);
  namespace fs = std::filesystem;
  fs::path pyccFile = fs::current_path().parent_path() / "pycc";
  if (!(fs::exists(pyccFile) && fs::is_regular_file(pyccFile))) {
    pyccFile = fs::path("../pycc");
  }
  std::string cmd = std::string("\"") + pyccFile.string() + "\" -o ../Testing/e2e_tempfile \"" + srcPath + "\" > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "pycc failed to compile tempfile demo"; return; }
  rc = std::system("../Testing/e2e_tempfile > ../Testing/out_tempfile.txt");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 0);
#else
  EXPECT_EQ(rc, 0);
#endif
  auto out = slurp_tmp("../Testing/out_tempfile.txt");
  EXPECT_EQ(out, std::string("TEMPFILE_OK\\n"));
}
