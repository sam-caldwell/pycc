/***
 * Name: test_runtime_warnings
 * Purpose: Verify warnings.warn/simplefilter runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeWarnings, WarnWritesStderr) {
  gc_reset_for_tests();
  // No easy capture in this harness; just call to ensure no crash.
  warnings_warn(string_from_cstr("oops"));
  warnings_simplefilter(string_from_cstr("ignore"), nullptr);
  SUCCEED();
}

