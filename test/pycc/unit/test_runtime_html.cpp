/***
 * Name: test_runtime_html
 * Purpose: Verify html.escape/unescape runtime shims.
 */
#include <gtest/gtest.h>
#include "runtime/Runtime.h"

using namespace pycc::rt;

TEST(RuntimeHtml, EscapeUnescape) {
  gc_reset_for_tests();
  void* s = string_from_cstr("<&>\"'");
  void* e = html_escape(s, 1);
  std::string es(string_data(e), string_len(e));
  EXPECT_EQ(es, std::string("&lt;&amp;&gt;&quot;&#x27;"));
  void* u = html_unescape(e);
  std::string us(string_data(u), string_len(u));
  EXPECT_EQ(us, std::string("<&>\"'"));
}

