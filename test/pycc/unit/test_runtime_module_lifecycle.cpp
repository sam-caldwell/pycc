/***
 * Name: test_runtime_module_lifecycle
 * Purpose: Validate module registry helpers.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeModule, RegisterLoadedUnload) {
  gc_reset_for_tests();
  EXPECT_FALSE(rt_module_loaded("modA"));
  rt_module_register("modA");
  EXPECT_TRUE(rt_module_loaded("modA"));
  rt_module_unload("modA");
  EXPECT_FALSE(rt_module_loaded("modA"));
}

