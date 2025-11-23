/***
 * Name: test_runtime_uuid
 * Purpose: Verify uuid.uuid4 runtime shim structure.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"

using namespace pycc::rt;

TEST(RuntimeUUID, UUID4FormatAndVersion) {
  gc_reset_for_tests();
  void* u = uuid_uuid4();
  ASSERT_NE(u, nullptr);
  std::string s(string_data(u), string_len(u));
  ASSERT_EQ(s.size(), 36u);
  EXPECT_EQ(s[8], '-'); EXPECT_EQ(s[13], '-'); EXPECT_EQ(s[18], '-'); EXPECT_EQ(s[23], '-');
  EXPECT_EQ(s[14], '4'); // version nibble
  char var = s[19];
  EXPECT_TRUE(var=='8' || var=='9' || var=='a' || var=='b'); // variant
}

