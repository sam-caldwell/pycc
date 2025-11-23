/***
 * Name: test_execute_list_len
 * Purpose: Compile and run a program constructing a list, returning len(a)=3.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

TEST(ExecuteListLen, DISABLED_ReturnsThree) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir;
  for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = (demosDir / "e2e_run_listlen.py").string();
  std::string cmd = std::string("../pycc -o e2e_listlen ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  ASSERT_EQ(rc, 0) << "pycc failed to compile list-len example";

  rc = std::system("./e2e_listlen > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 3);
#else
  EXPECT_EQ(rc, 3 << 8);
#endif
}
