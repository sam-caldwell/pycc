/***
 * Name: test_runtime_channel_immutability
 * Purpose: Ensure only immutable payloads can be sent across channels.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeChannels, ImmutableOnly) {
  gc_reset_for_tests();
  auto* ch = chan_new(1);
  // Immutable payloads: str, boxed int
  void* s = string_from_cstr("hello");
  EXPECT_NO_THROW(chan_send(ch, s));
  (void)chan_recv(ch); // consume to avoid blocking on cap=1
  void* bi = box_int(42);
  EXPECT_NO_THROW(chan_send(ch, bi));
  (void)chan_recv(ch);

  // Mutable/list payload should raise
  void* lst = list_new(2);
  EXPECT_ANY_THROW(chan_send(ch, lst));
}
