/***
 * Name: test_parseargs_sad
 * Purpose: Exercise sad-path CLI parsing for invalid/missing/unknown cases.
 */
#include <gtest/gtest.h>
#include "cli/CLI.h"

using namespace pycc::cli;

TEST(CLI_Sad, UnknownOption) {
  const char* argv[] = {"pycc", "--unknown"};
  Options o; EXPECT_FALSE(ParseArgs(2, const_cast<char**>(argv), o));
}

TEST(CLI_Sad, MissingOutputArgument) {
  const char* argv[] = {"pycc", "-o"};
  Options o; EXPECT_FALSE(ParseArgs(2, const_cast<char**>(argv), o));
}

TEST(CLI_Sad, InputStartingWithDashWithoutEndOfOptions) {
  const char* argv[] = {"pycc", "-strange.py"};
  Options o; EXPECT_FALSE(ParseArgs(2, const_cast<char**>(argv), o));
}

TEST(CLI_Sad, Conflicting_S_and_c) {
  const char* argv[] = {"pycc", "-S", "-c", "m.py"};
  Options o; EXPECT_FALSE(ParseArgs(4, const_cast<char**>(argv), o));
}

TEST(CLI_Sad, ColorInvalidFallsBackToAuto) {
  const char* argv[] = {"pycc", "--color=weird", "file.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  EXPECT_EQ(o.color, ColorMode::Auto);
}

TEST(CLI_Sad, DiagContextNonNumericBecomesZero) {
  const char* argv[] = {"pycc", "--diag-context=abc", "file.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  EXPECT_EQ(o.diagContext, 0);
}

TEST(CLI_Sad, AstLogInvalidFallsBackBefore) {
  const char* argv[] = {"pycc", "--ast-log=invalid", "m.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  EXPECT_EQ(o.astLog, AstLogMode::Before);
}
