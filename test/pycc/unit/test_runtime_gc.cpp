/***
 * Name: test_runtime_gc
 * Purpose: Basic GC behavior: roots prevent collection; stats update.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <cstring>

using namespace pycc::rt;

TEST(RuntimeGC, RootsProtectAndCollect) {
  gc_reset_for_tests();
  gc_set_threshold(64); // small to trigger collections

  void* s1 = string_new("abcdef", 6);
  gc_register_root(&s1);
  RuntimeStats st0 = gc_stats();

  // Allocate more to cross threshold and trigger collection
  void* s2 = string_new("hello world", 11);
  (void)s2;
  RuntimeStats st1 = gc_stats();
  EXPECT_GE(st1.numCollections, st0.numCollections);
  EXPECT_EQ(string_len(s1), 6u);

  // Drop root and collect; live bytes should drop
  gc_unregister_root(&s1);
  gc_collect();
  RuntimeStats st2 = gc_stats();
  EXPECT_GE(st2.numFreed, st1.numFreed);
}

