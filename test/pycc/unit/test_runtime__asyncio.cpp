/***
 * Name: test_runtime__asyncio
 * Purpose: Cover _asyncio runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(Runtime_Asyncio, LoopFutureSetGetDoneSleep) {
  void* loop = asyncio_get_event_loop();
  ASSERT_NE(loop, nullptr);
  void* fut = asyncio_future_new();
  ASSERT_NE(fut, nullptr);
  EXPECT_FALSE(asyncio_future_done(fut));
  void* val = string_from_cstr("x");
  asyncio_future_set_result(fut, val);
  EXPECT_TRUE(asyncio_future_done(fut));
  void* r = asyncio_future_result(fut);
  EXPECT_EQ(r, val);
  asyncio_sleep(0.0);
}

