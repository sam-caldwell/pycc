/***
 * Name: pycc::tests::Sanity
 * Purpose: Ensure the GoogleTest harness is wired correctly.
 * Inputs: none
 * Outputs: Pass/fail test result.
 * Theory of Operation: A minimal assertion that always passes.
 */
#include <gtest/gtest.h>

TEST(Sanity, TrueIsTrue) {
  EXPECT_TRUE(true);
}

