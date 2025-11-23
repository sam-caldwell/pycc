/***
 * Name: test_runtime_channels_backpressure_close
 * Purpose: Cover channel backpressure (bounded capacity) and close semantics.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

struct SendTwoPayload { RtChannelHandle* ch; RtAtomicIntHandle* prog; };

static void entry_send_two(const void* buf, std::size_t len, void** /*ret*/, std::size_t* /*ret_len*/) {
  ASSERT_EQ(len, sizeof(SendTwoPayload));
  const auto* p = static_cast<const SendTwoPayload*>(buf);
  // Mark start
  (void)atomic_int_add_fetch(p->prog, 1);
  // Send first value (should succeed immediately with cap=1)
  chan_send(p->ch, box_int(1));
  (void)atomic_int_add_fetch(p->prog, 1); // first send completed
  // This send should block until receiver consumes one
  chan_send(p->ch, box_int(2));
  (void)atomic_int_add_fetch(p->prog, 1); // second send completed
}

TEST(RuntimeChannels, BackpressureAndClose) {
  gc_reset_for_tests();
  auto* ch = chan_new(1);
  auto* prog = atomic_int_new(0);
  SendTwoPayload pay{ch, prog};
  RtThreadHandle* th = rt_spawn(entry_send_two, &pay, sizeof(pay));

  // Wait until first send completed (prog == 2)
  for (int spins = 0; spins < 1000000 && atomic_int_load(prog) < 2; ++spins) { /* spin */ }
  ASSERT_GE(atomic_int_load(prog), 2);

  // Receive first; this should unblock sender's second send eventually
  void* v1 = chan_recv(ch);
  ASSERT_NE(v1, nullptr);
  EXPECT_EQ(box_int_value(v1), 1);

  // Wait for second send to complete (prog == 3)
  for (int spins = 0; spins < 1000000 && atomic_int_load(prog) < 3; ++spins) { /* spin */ }
  ASSERT_GE(atomic_int_load(prog), 3);
  void* v2 = chan_recv(ch);
  ASSERT_NE(v2, nullptr);
  EXPECT_EQ(box_int_value(v2), 2);

  // Close and verify further recv returns nullptr
  chan_close(ch);
  void* v3 = chan_recv(ch);
  EXPECT_EQ(v3, nullptr);

  (void)rt_join(th, nullptr, nullptr);
  rt_thread_handle_destroy(th);
}
