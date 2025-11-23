/***
 * Name: test_runtime_allocator_thread_local
 * Purpose: Exercise thread-local steal path from global free lists by allocating, collecting, and reallocating.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeAllocator, ThreadLocalStealAndReuse) {
  gc_reset_for_tests();
  gc_set_threshold(64);
  // Phase 1: allocate many small strings that will be collected
  for (int i = 0; i < 256; ++i) {
    void* s = string_from_cstr("x");
    (void)s;
  }
  // Collect to move freed blocks into global free lists
  gc_collect();
  // Phase 2: allocate the same shapes again; this should steal from global into thread-local cache.
  for (int i = 0; i < 128; ++i) {
    void* s = string_from_cstr("y");
    ASSERT_NE(s, nullptr);
  }
  // Sanity: stats are internally consistent
  RuntimeStats st = gc_stats();
  EXPECT_GE(st.bytesAllocated, st.bytesLive);
}

