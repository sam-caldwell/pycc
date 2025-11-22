/***
 * Name: test_codegen_collections_lowering
 * Purpose: Verify lowering of collections module helpers.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/Codegen.h"

using namespace pycc;

static std::string genIR(const char* src, const char* file="collections_lowering.py") {
  lex::Lexer L; L.pushString(src, file);
  parse::Parser P(L);
  auto mod = P.parseModule();
  return codegen::Codegen::generateIR(*mod);
}

TEST(CodegenCollections, DeclaresAndCalls) {
  const char* src = R"PY(
def main() -> int:
  a = [1,2,1]
  c = collections.Counter(a)
  p = [["a", 1], ["b", 2]]
  od = collections.OrderedDict(p)
  maps = [od]
  m = collections.ChainMap(maps)
  dd = collections.defaultdict("x")
  v = collections.defaultdict_get(dd, "k")
  collections.defaultdict_set(dd, "k", "y")
  return 0
)PY";
  auto ir = genIR(src);
  ASSERT_NE(ir.find("declare ptr @pycc_collections_counter(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_collections_ordered_dict(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_collections_chainmap(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_collections_defaultdict_new(ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare ptr @pycc_collections_defaultdict_get(ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("declare void @pycc_collections_defaultdict_set(ptr, ptr, ptr)"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_collections_counter(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_collections_ordered_dict(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_collections_chainmap(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_collections_defaultdict_new(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call ptr @pycc_collections_defaultdict_get(ptr"), std::string::npos);
  ASSERT_NE(ir.find("call void @pycc_collections_defaultdict_set(ptr"), std::string::npos);
}

