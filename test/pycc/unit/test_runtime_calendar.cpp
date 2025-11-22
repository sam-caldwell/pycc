/***
 * Name: test_runtime_calendar
 * Purpose: Verify calendar.isleap/monthrange runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeCalendar, LeapAndMonthrange) {
  EXPECT_EQ(calendar_isleap(2024), 1);
  EXPECT_EQ(calendar_isleap(2100), 0); // not leap
  EXPECT_EQ(calendar_isleap(2000), 1);
  void* feb2024 = calendar_monthrange(2024, 2);
  ASSERT_NE(feb2024, nullptr);
  // Monday=0..Sunday=6
  int wd = static_cast<int>(box_int_value(list_get(feb2024, 0)));
  int nd = static_cast<int>(box_int_value(list_get(feb2024, 1)));
  EXPECT_EQ(nd, 29);
  EXPECT_GE(wd, 0);
  EXPECT_LE(wd, 6);
}

