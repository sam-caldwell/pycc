/***
 * Name: test_runtime_spawn_channel_atomic
 * Purpose: Exercise scaffolding for threads, channels, and atomics.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <cstring>

using namespace pycc::rt;

struct Payload1 { RtChannelHandle* ch; long long value; };

static void entry_send_value(const void* buf, std::size_t len, void** /*ret*/, std::size_t* /*ret_len*/) {
  ASSERT_EQ(len, sizeof(Payload1));
  const auto* p = static_cast<const Payload1*>(buf);
  // Box an integer as bytes payload pointer surrogate (for scaffolding, send as pointer to boxed int)
  long long v = p->value;
  void* boxed = box_int(v);
  chan_send(p->ch, boxed);
}

TEST(RuntimeChannels, SpawnAndSendRecv) {
  gc_reset_for_tests();
  auto* ch = chan_new(1);
  Payload1 pay{ch, 42};
  RtThreadHandle* th = rt_spawn(entry_send_value, &pay, sizeof(pay));
  void* got = chan_recv(ch);
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(box_int_value(got), 42);
  void* rb = nullptr; std::size_t rl = 0; (void)rt_join(th, &rb, &rl);
  rt_thread_handle_destroy(th);
  chan_close(ch);
}

struct Payload2 { RtAtomicIntHandle* a; int iters; };
static void entry_inc_atomic(const void* buf, std::size_t len, void** /*ret*/, std::size_t* /*ret_len*/) {
  ASSERT_EQ(len, sizeof(Payload2));
  const auto* p = static_cast<const Payload2*>(buf);
  for (int i = 0; i < p->iters; ++i) { (void)atomic_int_add_fetch(p->a, 1); }
}

TEST(RuntimeAtomics, SpawnManyIncrement) {
  gc_reset_for_tests();
  auto* a = atomic_int_new(0);
  constexpr int kThreads = 4;
  constexpr int kIters = 1000;
  RtThreadHandle* th[kThreads]{};
  for (int i = 0; i < kThreads; ++i) {
    Payload2 p{a, kIters};
    th[i] = rt_spawn(entry_inc_atomic, &p, sizeof(p));
  }
  for (int i = 0; i < kThreads; ++i) { (void)rt_join(th[i], nullptr, nullptr); rt_thread_handle_destroy(th[i]); }
  EXPECT_EQ(atomic_int_load(a), kThreads * kIters);
}

