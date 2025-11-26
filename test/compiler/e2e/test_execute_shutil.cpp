/***
 * Name: test_execute_shutil
 * Purpose: Compile and run a program using shutil; verify stdout and exit code.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static std::string readAll(const std::string& p) {
  std::ifstream in(p);
  std::string s, line; while (std::getline(in, line)) { s += line; s += "\n"; }
  return s;
}

TEST(ExecuteShutil, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_shutil.py").string();
  const bool atRepoRoot = fs::exists("build/pycc");
  fs::path outDir = atRepoRoot ? fs::path("build/Testing") : fs::path("../Testing");
  std::error_code ec; fs::create_directory(outDir, ec);
  fs::path pyccFile = atRepoRoot ? fs::path("build/pycc") : (fs::current_path().parent_path() / "pycc");
  fs::path outBin = outDir / "e2e_shutil";
  std::string cmd = std::string("\"") + pyccFile.string() + "\" -o \"" + outBin.string() + "\" \"" + srcPath + "\"";
  int rc = std::system(cmd.c_str());
  if (rc != 0) {
    // Fallback: try with relative compiler path to avoid any corner cases
    std::string cmd2 = std::string("../pycc -o \"") + outBin.string() + "\" \"" + srcPath + "\"";
    rc = std::system(cmd2.c_str());
  }
  if (rc != 0) {
    std::ostringstream oss; oss << fs::current_path();
    ASSERT_EQ(rc, 0) << "pycc failed to compile shutil demo using cmd: " << cmd << "; cwd=" << oss.str();
  }
  fs::path outTxt = outDir / "out_shutil.txt";
  rc = std::system((std::string("\"") + outBin.string() + "\" > \"" + outTxt.string() + "\" 2>/dev/null").c_str());
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 0);
#else
  EXPECT_EQ(rc, 0);
#endif
  auto out = readAll(outTxt.string());
  EXPECT_EQ(out, std::string("SHUTIL_OK\\n\n"));
}
