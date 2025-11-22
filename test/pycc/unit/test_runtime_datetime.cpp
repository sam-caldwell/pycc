/***
 * Name: test_runtime_datetime
 * Purpose: Validate datetime shims (now/utcnow/fromtimestamp/utcfromtimestamp) return ISO-8601 strings.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"
#include <cstring>

using namespace pycc::rt;

static bool looks_iso8601(const char* s) {
  return s && std::strlen(s) >= 19 && s[4]=='-' && s[7]=='-' && s[10]=='T' && s[13]==':' && s[16]==':';
}

TEST(RuntimeDatetime, NowAndUtcnowFormat) {
  void* n = datetime_now();
  void* u = datetime_utcnow();
  ASSERT_TRUE(looks_iso8601(string_data(n)));
  ASSERT_TRUE(looks_iso8601(string_data(u)));
}

TEST(RuntimeDatetime, EpochUtcFromTimestamp) {
  void* u = datetime_utcfromtimestamp(0.0);
  ASSERT_STREQ(string_data(u), "1970-01-01T00:00:00");
}

