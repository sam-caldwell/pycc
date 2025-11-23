/***
 * Name: test_runtime_errno
 * Purpose: Verify errno constants match system values.
 */
#include <gtest/gtest.h>
#include "runtime/All.h"
#include <cerrno>

using namespace pycc::rt;

TEST(RuntimeErrno, ValuesMatchSystem) {
  EXPECT_EQ(errno_EPERM(), EPERM);
  EXPECT_EQ(errno_ENOENT(), ENOENT);
  EXPECT_EQ(errno_EEXIST(), EEXIST);
#ifdef EISDIR
  EXPECT_EQ(errno_EISDIR(), EISDIR);
#endif
#ifdef ENOTDIR
  EXPECT_EQ(errno_ENOTDIR(), ENOTDIR);
#endif
  EXPECT_EQ(errno_EACCES(), EACCES);
}

