/***
 * Name: test_cli
 * Purpose: Basic CLI option parsing and negative cases.
 */
#include <gtest/gtest.h>
#include "cli/CLI.h"

using namespace pycc::cli;

TEST(CLI, HelpAndOutput) {
  const char* argv[] = {"pycc", "-h", "-o", "out", "file.py"};
  Options o;
  ASSERT_TRUE(ParseArgs(5, const_cast<char**>(argv), o));
  EXPECT_TRUE(o.showHelp);
  EXPECT_EQ(o.outputFile, "out");
  ASSERT_EQ(o.inputs.size(), 1u);
  EXPECT_EQ(o.inputs[0], "file.py");
}

TEST(CLI, Conflict_S_and_c) {
  const char* argv[] = {"pycc", "-S", "-c", "file.py"};
  Options o;
  EXPECT_FALSE(ParseArgs(4, const_cast<char**>(argv), o));
}

TEST(CLI, UnknownOption) {
  const char* argv[] = {"pycc", "--unknown"};
  Options o;
  EXPECT_FALSE(ParseArgs(2, const_cast<char**>(argv), o));
}

TEST(CLI, MetricsJsonFlag) {
  const char* argv[] = {"pycc", "--metrics-json", "file.py"};
  Options o;
  ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  EXPECT_TRUE(o.metricsJson);
  ASSERT_EQ(o.inputs.size(), 1u);
  EXPECT_EQ(o.inputs[0], "file.py");
}

