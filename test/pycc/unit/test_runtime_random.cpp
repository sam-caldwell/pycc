/***
 * Name: test_runtime_random
 * Purpose: Verify random.random/randint/seed runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeRandom, SeedReproducibility) {
  gc_reset_for_tests();
  random_seed(42);
  double r1 = random_random();
  int32_t i1 = random_randint(1, 10);
  random_seed(42);
  double r2 = random_random();
  int32_t i2 = random_randint(1, 10);
  EXPECT_DOUBLE_EQ(r1, r2);
  EXPECT_EQ(i1, i2);
  EXPECT_GE(r1, 0.0); EXPECT_LT(r1, 1.0);
  EXPECT_GE(i1, 1); EXPECT_LE(i1, 10);
}

