/***
 * Name: test_runtime_collections
 * Purpose: Cover collections helpers: Counter, OrderedDict, ChainMap, defaultdict_*.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;
extern "C" void* pycc_dict_iter_new(void* dict);
extern "C" void* pycc_dict_iter_next(void* it);

TEST(RuntimeCollections, CounterCountsIntsAndStrings) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, box_int(2));
  list_push_slot(&lst, box_int(1));
  void* d = collections_counter(lst);
  // Iterate keys and verify only "1" and "2" present with correct counts
  void* it = pycc_dict_iter_new(d);
  int seen = 0;
  while (true) {
    void* k = pycc_dict_iter_next(it);
    if (!k) break;
    const char* s = string_data(k);
    if (std::strcmp(s, "1") == 0) {
      EXPECT_EQ(box_int_value(dict_get(d, k)), 2);
    } else if (std::strcmp(s, "2") == 0) {
      EXPECT_EQ(box_int_value(dict_get(d, k)), 1);
    } else {
      FAIL() << "Unexpected key in Counter: " << s;
    }
    ++seen;
  }
  EXPECT_EQ(seen, 2);

  void* ls = list_new(0);
  void* sA = string_from_cstr("a");
  list_push_slot(&ls, sA); list_push_slot(&ls, sA);
  void* ds = collections_counter(ls);
  EXPECT_EQ(box_int_value(dict_get(ds, sA)), 2);
}

TEST(RuntimeCollections, OrderedDictFromPairsAndChainMap) {
  gc_reset_for_tests();
  void* pairs = list_new(0);
  void* p1 = list_new(2);
  void* k1 = string_from_cstr("a");
  list_push_slot(&p1, k1);
  list_push_slot(&p1, box_int(1));
  list_push_slot(&pairs, p1);
  void* p2 = list_new(2);
  void* k2 = string_from_cstr("b");
  list_push_slot(&p2, k2);
  list_push_slot(&p2, box_int(2));
  list_push_slot(&pairs, p2);
  void* od = collections_ordered_dict(pairs);
  ASSERT_EQ(dict_len(od), 2u);
  EXPECT_EQ(box_int_value(dict_get(od, k1)), 1);
  EXPECT_EQ(box_int_value(dict_get(od, k2)), 2);

  // ChainMap with single dict list should mirror dict
  void* dicts = list_new(1);
  list_push_slot(&dicts, od);
  void* merged = collections_chainmap(dicts);
  ASSERT_EQ(dict_len(merged), 2u);
  EXPECT_EQ(box_int_value(dict_get(merged, k2)), 2);
}

TEST(RuntimeCollections, DefaultDictGetAndSet) {
  gc_reset_for_tests();
  void* dd = collections_defaultdict_new(string_from_cstr("x"));
  void* key = string_from_cstr("k");
  void* v1 = collections_defaultdict_get(dd, key);
  ASSERT_NE(v1, nullptr);
  EXPECT_EQ(std::memcmp(string_data(v1), "x", 1), 0);
  // Now set a new value and read back
  collections_defaultdict_set(dd, key, string_from_cstr("y"));
  // Access via API: getting again should return 'y'
  void* v2 = collections_defaultdict_get(dd, key);
  EXPECT_EQ(std::memcmp(string_data(v2), "y", 1), 0);
}
