/***
 * Name: test_runtime_pprint
 * Purpose: Verify pprint.pformat runtime shim on lists and scalars.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimePprint, FormatsListAndNested) {
  gc_reset_for_tests();
  void* lst = list_new(0);
  list_push_slot(&lst, box_int(1));
  list_push_slot(&lst, string_from_cstr("x"));
  void* s = pprint_pformat(lst);
  std::string out(string_data(s), string_len(s));
  EXPECT_EQ(out, std::string("[1, 'x']"));
  // nested
  void* inner = list_new(0);
  list_push_slot(&inner, box_int(2));
  list_push_slot(&lst, inner);
  void* s2 = pprint_pformat(lst);
  std::string out2(string_data(s2), string_len(s2));
  EXPECT_NE(out2.find("[1, 'x', [2]]"), std::string::npos);
}

