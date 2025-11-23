/***
 * Name: test_runtime_atomics_contention
 * Purpose: Stress atomic add under high contention across many threads.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

struct IncPayload { RtAtomicIntHandle* a; int iters; };
static void entry_inc_many(const void* buf, std::size_t len, void** /*ret*/, std::size_t* /*ret_len*/) {
  ASSERT_EQ(len, sizeof(IncPayload));
  const auto* p = static_cast<const IncPayload*>(buf);
  for (int i = 0; i < p->iters; ++i) { (void)atomic_int_add_fetch(p->a, 1); }
}

TEST(RuntimeAtomics, HighContention) {
  gc_reset_for_tests();
  auto* a = atomic_int_new(0);
  constexpr int kThreads = 8;
  constexpr int kIters = 5000;
  RtThreadHandle* th[kThreads]{};
  for (int i = 0; i < kThreads; ++i) {
    IncPayload p{a, kIters};
    th[i] = rt_spawn(entry_inc_many, &p, sizeof(p));
  }
  for (int i = 0; i < kThreads; ++i) { (void)rt_join(th[i], nullptr, nullptr); rt_thread_handle_destroy(th[i]); }
  EXPECT_EQ(atomic_int_load(a), kThreads * kIters);
}

