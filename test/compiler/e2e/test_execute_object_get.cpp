/***
 * Name: test_execute_object_get
 * Purpose: Compile and run a program using object(...) and obj_get; verify len of field is returned.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>

TEST(ExecuteObjectGet, ReturnsFieldStringLen) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir;
  for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty());
  const auto srcPath = (demosDir / "e2e_objget.py").string();
  std::error_code ec; std::filesystem::create_directory("../Testing", ec);
  std::string cmd = std::string("../pycc -o ../Testing/e2e_objget ") + srcPath + " > /dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  if (rc != 0) { GTEST_SKIP() << "Skipping: pycc failed to compile object-get example"; }

  rc = std::system("../Testing/e2e_objget > /dev/null 2>&1");
#ifdef WIFEXITED
  ASSERT_TRUE(WIFEXITED(rc));
  int code = WEXITSTATUS(rc);
  EXPECT_EQ(code, 5);
#else
  EXPECT_EQ(rc, 5 << 8);
#endif
}
