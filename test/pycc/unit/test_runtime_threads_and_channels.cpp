/***
 * Name: test_runtime_threads_and_channels
 * Purpose: Cover thread join return marshaling and channel close unblocking recv.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <cstring>

using namespace pycc::rt;

static void start_return(const void* /*payload*/, std::size_t /*len*/, void** ret, std::size_t* ret_len) {
  static const char msg[] = "OK";
  *ret_len = sizeof(msg);
  *ret = const_cast<char*>(msg);
}

TEST(RuntimeThreads, JoinReturnsPayloadCopy) {
  gc_reset_for_tests();
  RtThreadHandle* th = rt_spawn(start_return, nullptr, 0);
  ASSERT_NE(th, nullptr);
  void* buf = nullptr; std::size_t n = 0;
  ASSERT_TRUE(rt_join(th, &buf, &n));
  ASSERT_NE(buf, nullptr);
  ASSERT_GE(n, 2u);
  // Expect a copy; string must start with "OK"
  EXPECT_EQ(std::memcmp(buf, "OK", 2), 0);
  std::free(buf);
  rt_thread_handle_destroy(th);
}

struct CloseOnDelayPayload { RtChannelHandle* ch; };
static void start_close_after(const void* payload, std::size_t len, void** /*ret*/, std::size_t* /*ret_len*/) {
  ASSERT_EQ(len, sizeof(CloseOnDelayPayload));
  const auto* p = static_cast<const CloseOnDelayPayload*>(payload);
  // brief bounded spin to ensure receiver is waiting
  for (int i = 0; i < 100000; ++i) { /* spin */ }
  chan_close(p->ch);
}

TEST(RuntimeChannels, CloseUnblocksRecv) {
  gc_reset_for_tests();
  auto* ch = chan_new(1);
  CloseOnDelayPayload pl{ch};
  RtThreadHandle* th = rt_spawn(start_close_after, &pl, sizeof(pl));
  // Receive on empty channel; should unblock with nullptr once closed by the other thread
  void* v = chan_recv(ch);
  EXPECT_EQ(v, nullptr);
  (void)rt_join(th, nullptr, nullptr);
  rt_thread_handle_destroy(th);
}
