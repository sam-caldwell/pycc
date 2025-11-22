/***
 * Name: test_runtime_colorsys
 * Purpose: Verify colorsys.rgb_to_hsv/hsv_to_rgb runtime shims.
 */
#include <gtest/gtest.h>
#include <cmath>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeColorsys, RoundTrip) {
  gc_reset_for_tests();
  // Red -> HSV -> back
  void* hsv = colorsys_rgb_to_hsv(1.0, 0.0, 0.0);
  ASSERT_EQ(list_len(hsv), 3u);
  double h = box_float_value(list_get(hsv, 0));
  double s = box_float_value(list_get(hsv, 1));
  double v = box_float_value(list_get(hsv, 2));
  EXPECT_NEAR(h, 0.0, 1e-9);
  EXPECT_NEAR(s, 1.0, 1e-9);
  EXPECT_NEAR(v, 1.0, 1e-9);
  void* rgb = colorsys_hsv_to_rgb(h, s, v);
  ASSERT_EQ(list_len(rgb), 3u);
  EXPECT_NEAR(box_float_value(list_get(rgb, 0)), 1.0, 1e-9);
  EXPECT_NEAR(box_float_value(list_get(rgb, 1)), 0.0, 1e-9);
  EXPECT_NEAR(box_float_value(list_get(rgb, 2)), 0.0, 1e-9);
}

