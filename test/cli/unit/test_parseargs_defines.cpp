/***
 * Name: test_parseargs_defines
 * Purpose: Ensure -DNAME and -DNAME=VALUE are parsed into Options::defines.
 */
#include <gtest/gtest.h>
#include "cli/CLI.h"

using namespace pycc::cli;

TEST(CLI_Defines, DefineNameOnly) {
  const char* argv[] = {"pycc", "-DOPT_ELIDE_GCBARRIER", "file.py"};
  Options o;
  ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  ASSERT_FALSE(o.defines.empty());
  bool found = false;
  for (const auto& d : o.defines) if (d == "OPT_ELIDE_GCBARRIER") { found = true; break; }
  EXPECT_TRUE(found);
}

TEST(CLI_Defines, DefineNameEqualsValue) {
  const char* argv[] = {"pycc", "-DDEBUG_LEVEL=2", "main.py"};
  Options o;
  ASSERT_TRUE(ParseArgs(3, const_cast<char**>(argv), o));
  ASSERT_EQ(o.defines.size(), 1u);
  EXPECT_EQ(o.defines[0], std::string("DEBUG_LEVEL=2"));
}

