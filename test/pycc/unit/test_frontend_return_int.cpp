/***
 * Name: pycc::tests::FrontendReturnInt
 * Purpose: Validate frontend::ParseReturnInt extracts the integer literal.
 * Inputs: none
 * Outputs: Pass/fail test
 * Theory of Operation: Provide a minimal module, assert parsed value.
 */
#include <gtest/gtest.h>

#include <string>

#include "pycc/frontend/simple_return_int.h"

using namespace pycc;

TEST(FrontendReturnInt, ParsesConstantReturn) {
  const std::string src = "def main() -> int:\n    return 42\n";
  int value = 0;
  std::string err;
  ASSERT_TRUE(frontend::ParseReturnInt(src, value, err)) << err;
  EXPECT_EQ(42, value);
}

