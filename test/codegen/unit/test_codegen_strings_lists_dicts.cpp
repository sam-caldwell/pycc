/***
 * Name: test_codegen_strings_lists_dicts
 * Purpose: Ensure IR calls into runtime for strings, lists, and dicts; and write barriers are emitted.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src) {
  lex::Lexer L; L.pushString(src, "cont.py");
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenContainers, StringAndListAndDictOpsPresent) {
  const char* src = R"PY(
def main() -> int:
  s = "ab"
  # concat
  t = s + "cd"
  # index -> single-char slice
  c = s[0]
  # contains
  b = ("a" in s)
  # repeat
  r = s * 3
  # list literal + append + get/set + len
  xs = [1,2]
  xs.append(3)
  u = xs[0]
  xs[0] = 4
  n = len(xs)
  # dict literal + get/set + len
  d = {"a": 1}
  v = d["a"]
  d["b"] = 2
  m = len(d)
  return 0
)PY";
  const auto ir = genIR(src);
  // Strings
  EXPECT_NE(ir.find("@pycc_string_new"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_string_concat"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_string_slice"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_string_contains"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_string_repeat"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_string_eq"), std::string::npos);
  // Lists
  EXPECT_NE(ir.find("@pycc_list_new"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_list_push"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_list_get"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_list_set"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_list_len"), std::string::npos);
  // Dicts
  EXPECT_NE(ir.find("@pycc_dict_new"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_dict_set"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_dict_get"), std::string::npos);
  EXPECT_NE(ir.find("@pycc_dict_len"), std::string::npos);
  // GC write barrier on pointer stores
  EXPECT_NE(ir.find("@pycc_gc_write_barrier"), std::string::npos);
}

