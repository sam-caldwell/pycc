/***
 * Name: test_usage
 * Purpose: Validate Usage() content exposes documented flags.
 */
#include <gtest/gtest.h>
#include "cli/Usage.h"

using namespace pycc::cli;

TEST(CLI_Usage, ContainsExpectedFlags) {
  auto u = Usage();
  EXPECT_NE(u.find("pycc [options] file"), std::string::npos);
  EXPECT_NE(u.find("-o <file>"), std::string::npos);
  EXPECT_NE(u.find("-S"), std::string::npos);
  EXPECT_NE(u.find("-c"), std::string::npos);
  EXPECT_NE(u.find("--metrics"), std::string::npos);
  EXPECT_NE(u.find("--metrics-json"), std::string::npos);
  EXPECT_NE(u.find("--opt-const-fold"), std::string::npos);
  EXPECT_NE(u.find("--color="), std::string::npos);
  EXPECT_NE(u.find("--diag-context="), std::string::npos);
  EXPECT_NE(u.find("--                    End of options"), std::string::npos);
}

