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

TEST(RuntimeDatetime, FromTimestampLocalEpochPrefix) {
  void* l0 = datetime_fromtimestamp(0.0);
  const char* s = string_data(l0);
  ASSERT_TRUE(s != nullptr);
  // Prefix matches epoch day in local time (time component may vary with timezone).
  // Accept either 1970-01-01 or 1969-12-31 (depending on local tz offset).
  bool ok = (std::strncmp(s, "1970-01-01T", 11) == 0) || (std::strncmp(s, "1969-12-31T", 11) == 0);
  ASSERT_TRUE(ok);
}

TEST(RuntimeDatetime, FromTimestampAcceptsIntAndFloat) {
  // int
  void* l1 = datetime_fromtimestamp(0);
  ASSERT_TRUE(looks_iso8601(string_data(l1)));
  // float
  void* l2 = datetime_fromtimestamp(0.5);
  ASSERT_TRUE(looks_iso8601(string_data(l2)));
}
