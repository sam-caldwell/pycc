/***
 * Name: test_runtime_itertools
 * Purpose: Cover itertools materialized helpers in the runtime.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

extern "C" void* pycc_list_get(void* list, long long index);

static void* mk_int_list(std::initializer_list<long long> vals) {
  void* lst = list_new(vals.size());
  for (auto v : vals) list_push_slot(&lst, box_int(v));
  return lst;
}

TEST(RuntimeItertools, ChainAndChainFromIterable) {
  gc_reset_for_tests();
  void* a = mk_int_list({1,2});
  void* b = mk_int_list({3});
  void* c = itertools_chain2(a,b);
  ASSERT_EQ(list_len(c), 3u);
  EXPECT_EQ(box_int_value(pycc_list_get(c, 0)), 1);
  EXPECT_EQ(box_int_value(pycc_list_get(c, 2)), 3);

  void* one = mk_int_list({1});
  void* two = mk_int_list({2});
  void* ll = list_new(2);
  list_push_slot(&ll, one);
  list_push_slot(&ll, two);
  void* flat = itertools_chain_from_iterable(ll);
  ASSERT_EQ(list_len(flat), 2u);
  EXPECT_EQ(box_int_value(pycc_list_get(flat, 0)), 1);
  EXPECT_EQ(box_int_value(pycc_list_get(flat, 1)), 2);
}

TEST(RuntimeItertools, ProductPermutationsCombinations) {
  gc_reset_for_tests();
  void* a = mk_int_list({1,2});
  void* b = mk_int_list({3});
  void* prod = itertools_product2(a,b);
  ASSERT_EQ(list_len(prod), 2u);
  void* p0 = pycc_list_get(prod, 0);
  ASSERT_EQ(list_len(p0), 2u);
  EXPECT_EQ(box_int_value(pycc_list_get(p0, 0)), 1);
  EXPECT_EQ(box_int_value(pycc_list_get(p0, 1)), 3);

  void* perm = itertools_permutations(mk_int_list({1,2,3}), 2);
  ASSERT_EQ(list_len(perm), 6u);

  void* comb = itertools_combinations(mk_int_list({1,2,3}), 2);
  ASSERT_EQ(list_len(comb), 3u);

  void* cwr = itertools_combinations_with_replacement(mk_int_list({1,2}), 2);
  ASSERT_EQ(list_len(cwr), 3u);
}

TEST(RuntimeItertools, ZipLongestIslice) {
  gc_reset_for_tests();
  void* a = mk_int_list({1,2});
  void* b = mk_int_list({3,4,5});
  void* X = string_from_cstr("X");
  void* z = itertools_zip_longest2(a,b,X);
  ASSERT_EQ(list_len(z), 3u);
  void* last = pycc_list_get(z, 2);
  ASSERT_EQ(list_len(last), 2u);
  // first from a missing => fillvalue
  EXPECT_EQ(std::memcmp(string_data(pycc_list_get(last, 0)), string_data(X), string_len(X)), 0);
  EXPECT_EQ(box_int_value(pycc_list_get(last, 1)), 5);

  void* rng = mk_int_list({0,1,2,3,4,5,6,7,8,9});
  void* sl = itertools_islice(rng, 2, 8, 2);
  ASSERT_EQ(list_len(sl), 3u);
  EXPECT_EQ(box_int_value(pycc_list_get(sl, 0)), 2);
  EXPECT_EQ(box_int_value(pycc_list_get(sl, 2)), 6);
}

TEST(RuntimeItertools, AccumulateRepeatPairwiseBatchedCompress) {
  gc_reset_for_tests();
  void* xs = mk_int_list({1,2,3});
  void* acc = itertools_accumulate_sum(xs);
  ASSERT_EQ(list_len(acc), 3u);
  EXPECT_EQ(box_int_value(pycc_list_get(acc, 2)), 6);

  void* R = itertools_repeat(string_from_cstr("k"), 3);
  ASSERT_EQ(list_len(R), 3u);
  EXPECT_EQ(std::memcmp(string_data(pycc_list_get(R, 0)), "k", 1), 0);
  EXPECT_EQ(std::memcmp(string_data(pycc_list_get(R, 2)), "k", 1), 0);

  void* pw = itertools_pairwise(mk_int_list({1,2,3,4}));
  ASSERT_EQ(list_len(pw), 3u);
  void* pair0 = pycc_list_get(pw, 0);
  ASSERT_EQ(list_len(pair0), 2u);
  EXPECT_EQ(box_int_value(pycc_list_get(pair0, 0)), 1);
  EXPECT_EQ(box_int_value(pycc_list_get(pair0, 1)), 2);

  void* bat = itertools_batched(mk_int_list({1,2,3,4,5}), 2);
  ASSERT_EQ(list_len(bat), 3u);
  EXPECT_EQ(list_len(pycc_list_get(bat, 0)), 2u);
  EXPECT_EQ(list_len(pycc_list_get(bat, 2)), 1u);

  void* comp = itertools_compress(mk_int_list({10,20,30}), mk_int_list({1,0,1}));
  ASSERT_EQ(list_len(comp), 2u);
  EXPECT_EQ(box_int_value(pycc_list_get(comp, 0)), 10);
  EXPECT_EQ(box_int_value(pycc_list_get(comp, 1)), 30);
}

