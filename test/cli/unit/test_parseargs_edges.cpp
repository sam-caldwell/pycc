/***
 * Name: test_parseargs_edges
 * Purpose: Exercise CLI parsing edge cases to grow coverage.
 */
#include <gtest/gtest.h>
#include "cli/CLI.h"

using namespace pycc::cli;

TEST(CLI_Edges, LogPathEmptyValueAccepted) {
  const char* argv[] = {"pycc", "--log-path=", "file.py"};
  Options o; ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  EXPECT_EQ(o.logPath, std::string(""));
  ASSERT_EQ(o.inputs.size(), 1u);
}

TEST(CLI_Edges, DoubleDashTreatsAllFollowingAsPositional) {
  const char* argv[] = {"pycc", "--", "-S", "--metrics", "x.py"};
  Options o; ASSERT_TRUE(ParseArgs(5, const_cast<char**>(argv), o));
  ASSERT_EQ(o.inputs.size(), 3u);
  EXPECT_EQ(o.inputs[0], "-S");
  EXPECT_EQ(o.inputs[1], "--metrics");
  EXPECT_EQ(o.inputs[2], "x.py");
  EXPECT_FALSE(o.emitAssemblyOnly);
  EXPECT_FALSE(o.metrics);
}

TEST(CLI_Edges, LastOutputFlagWins) {
  const char* argv[] = {"pycc", "-o", "a", "-o", "b", "main.py"};
  Options o; ASSERT_TRUE(ParseArgs(6, const_cast<char**>(argv), o));
  EXPECT_EQ(o.outputFile, std::string("b"));
}

