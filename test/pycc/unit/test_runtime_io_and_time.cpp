/***
 * Name: test_runtime_io_and_time
 * Purpose: Cover stdout/stderr write and time API monotonic property.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <thread>
#include <chrono>

using namespace pycc::rt;

TEST(RuntimeIO, StdoutStderrWriteNoCrash) {
  gc_reset_for_tests();
  void* s = string_from_cstr("hello\n");
  // These write to process stdout/stderr; just ensure they don't crash.
  io_write_stdout(s);
  io_write_stderr(s);
}

TEST(RuntimeOS, TimeMonotonicNonDecreasing) {
  const auto t1 = os_time_ms();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  const auto t2 = os_time_ms();
  EXPECT_LE(t1, t2);
}

