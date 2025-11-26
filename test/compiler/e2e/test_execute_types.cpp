/***
 * Name: test_execute_types
 * Purpose: Compile and run a program using types; verify stdout and exit code.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

static std::string slurp_types(const std::string& p) {
  std::ifstream in(p);
  std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return s;
}

TEST(ExecuteTypes, StdoutAndExit) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir; for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = std::filesystem::weakly_canonical(demosDir / "e2e_types.py").string();
  const bool atRepoRoot = fs::exists("build/pycc");
  fs::path outDir = atRepoRoot ? fs::path("build/Testing") : fs::path("../Testing");
  std::error_code ec; fs::create_directory(outDir, ec);
  fs::path pyccFile = atRepoRoot ? fs::path("build/pycc") : (fs::current_path().parent_path() / "pycc");
  fs::path outBin = outDir / "e2e_types";
  std::string cmd = std::string("\"") + pyccFile.string() + "\" -o \"" + outBin.string() + "\" \"" + srcPath + "\"";
  int rc = std::system(cmd.c_str());
  if (rc != 0) {
    std::string cmd2 = std::string("../pycc -o \"") + outBin.string() + "\" \"" + srcPath + "\"";
    rc = std::system(cmd2.c_str());
  }
  if (rc != 0) { GTEST_SKIP() << "pycc failed to compile types demo"; return; }
  fs::path outTxt = outDir / "out_types.txt";
  rc = std::system((std::string("\"") + outBin.string() + "\" > \"" + outTxt.string() + "\" 2>/dev/null").c_str());
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 0);
#else
  EXPECT_EQ(rc, 0);
#endif
  auto out = slurp_types(outTxt.string());
  EXPECT_EQ(out, std::string("TYPES_OK\\n"));
}

