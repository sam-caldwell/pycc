/***
 * Name: test_use_env_color
 * Purpose: Verify Compiler::use_env_color respects PYCC_COLOR values.
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include "compiler/Compiler.h"

static void set_env(const char* k, const char* v) {
#if defined(_WIN32)
  std::string pair = std::string(k) + "=" + (v ? v : "");
  _putenv(pair.c_str());
#else
  if (v) { setenv(k, v, 1); } else { unsetenv(k); }
#endif
}

TEST(UseEnvColor, DefaultsFalseWhenUnset) {
  set_env("PYCC_COLOR", nullptr);
  EXPECT_FALSE(pycc::Compiler::use_env_color());
}

TEST(UseEnvColor, RecognizesTrueValues) {
  set_env("PYCC_COLOR", "1"); EXPECT_TRUE(pycc::Compiler::use_env_color());
  set_env("PYCC_COLOR", "true"); EXPECT_TRUE(pycc::Compiler::use_env_color());
  set_env("PYCC_COLOR", "Yes"); EXPECT_TRUE(pycc::Compiler::use_env_color());
}

TEST(UseEnvColor, RecognizesFalseValues) {
  set_env("PYCC_COLOR", "0"); EXPECT_FALSE(pycc::Compiler::use_env_color());
  set_env("PYCC_COLOR", "false"); EXPECT_FALSE(pycc::Compiler::use_env_color());
  set_env("PYCC_COLOR", "no"); EXPECT_FALSE(pycc::Compiler::use_env_color());
}

