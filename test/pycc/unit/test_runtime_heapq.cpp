/***
 * Name: test_runtime_heapq
 * Purpose: Verify heapq.heappush/heappop runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeHeapq, PushPopOrder) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  heapq_heappush(lst, box_int(3));
  heapq_heappush(lst, box_int(1));
  heapq_heappush(lst, box_int(4));
  heapq_heappush(lst, box_int(2));
  void* a = heapq_heappop(lst);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(box_int_value(a), 1);
  void* b = heapq_heappop(lst);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(box_int_value(b), 2);
}

