/***
 * Name: test_runtime_threads_shared
 * Purpose: Basic cross-thread sharing: read-only access to a string object.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <thread>

using namespace pycc::rt;

TEST(RuntimeThreads, ShareStringReadOnly) {
  gc_reset_for_tests();
  gc_set_background(false); // deterministic
  void* s = string_from_cstr("shared");
  gc_register_root(&s); // keep alive across threads
  std::size_t len_in_thread = 0;
  std::thread t([&]() { len_in_thread = string_len(s); });
  t.join();
  EXPECT_EQ(len_in_thread, 6u);
  gc_unregister_root(&s);
}
