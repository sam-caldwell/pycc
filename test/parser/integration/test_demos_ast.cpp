/***
 * Name: test_demos_ast
 * Purpose: Verify that Python code in demos directory can be tokenized, parsed,
 *          and yields expected AST structures before and after optimization.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "optimizer/ConstantFold.h"
#include "optimizer/AlgebraicSimplify.h"
#include "optimizer/DCE.h"
#include "observability/AstPrinter.h"

using namespace pycc;
namespace fs = std::filesystem;

struct Expectations {
  std::vector<std::string> beforeContains;
  std::vector<std::string> afterContains;
};

// reserved for future file-content checks

static void assert_contains_all(const std::string& hay, const std::vector<std::string>& needles, const char* where) {
  for (const auto& n : needles) {
    ASSERT_NE(hay.find(n), std::string::npos) << where << " missing: '" << n << "'\n" << hay;
  }
}

TEST(DemosAst, AllDemosParseAndMatchExpectedAstStructures) {
  // Resolve demos directory relative to test working dir.
  // For integration tests, working dir is build/tmp...; demos is three levels up.
  std::vector<fs::path> candidates = {fs::path("../../../demos"), fs::path("../../demos"), fs::path("demos")};
  fs::path demosDir;
  for (const auto& c : candidates) { if (fs::exists(c)) { demosDir = c; break; } }
  ASSERT_FALSE(demosDir.empty()) << "demos directory not found in expected relative locations";

  // Expectations per filename (simple substrings from AstPrinter output)
  std::unordered_map<std::string, Expectations> exp{
    {"minimal.py",   {{"Module", "FunctionDef name=main", "ReturnStmt"}, {"Module", "FunctionDef name=main", "ReturnStmt"}}},
    {"sample.py",    {{"FunctionDef name=main", "ReturnStmt"}, {"FunctionDef name=main", "ReturnStmt"}}},
    {"arith.py",     {{"FunctionDef name=add", "FunctionDef name=main", "AssignStmt target=y", "Binary"}, {"FunctionDef name=add", "FunctionDef name=main", "AssignStmt target=y", "IntLiteral 20"}}},
    {"boolexpr.py",  {{"FunctionDef name=main", "IfStmt", "BoolLiteral True", "BoolLiteral False"}, {"FunctionDef name=main", "IfStmt"}}},
    {"recursion.py", {{"FunctionDef name=fact", "FunctionDef name=main", "IfStmt", "Call"}, {"FunctionDef name=fact", "FunctionDef name=main", "IfStmt", "Call"}}},
    {"collections.py", {{"FunctionDef name=main", "ListLiteral", "Call", "Name len"}, {"FunctionDef name=main", "Call", "Name len"}}},
    {"compare.py",   {{"FunctionDef name=main", "Binary"}, {"FunctionDef name=main"}}},
    {"loops.py",     {{"ForStmt", "WhileStmt"}, {"ForStmt", "WhileStmt"}}},
    {"augassign.py", {{"AugAssignStmt"}, {"AugAssignStmt"}}},
    {"comprehensions.py", {{"ListComp", "SetComp", "DictComp"}, {"ListComp", "SetComp", "DictComp"}}},
    {"try_except.py", {{"TryStmt", "ExceptHandler"}, {"TryStmt", "ExceptHandler"}}},
    {"classes.py",   {{"FunctionDef name=main"}, {"FunctionDef name=main"}}},
    {"match_case.py", {{"MatchStmt"}, {"MatchStmt"}}}
  };

  for (const auto& entry : fs::directory_iterator(demosDir)) {
    if (!entry.is_regular_file()) continue;
    const auto path = entry.path();
    if (path.extension() != ".py") continue;
    const auto name = path.filename().string();
    // Skip tiny parsing-exercise snippets that are not full demo programs
    if (name.rfind("pe_", 0) == 0) { continue; }
    SCOPED_TRACE(std::string("demo=") + name);

    // 1) Lex the file
    lex::Lexer L; L.pushFile(path.string());
    const auto toks = L.tokens();
    ASSERT_FALSE(toks.empty());

    // 2) Parse into AST
    parse::Parser P(L);
    auto mod = P.parseModule();
    ASSERT_TRUE(mod);
    ASSERT_GE(mod->functions.size(), 1u);

    // 3) Print AST before
    obs::AstPrinter printer;
    const auto before = printer.print(*mod);
    auto it = exp.find(name);
    if (it != exp.end()) {
      assert_contains_all(before, it->second.beforeContains, "before");
    } else {
      // Generic invariants
      ASSERT_NE(before.find("Module"), std::string::npos);
      ASSERT_NE(before.find("FunctionDef"), std::string::npos);
    }

    // 4) Run semantic optimizations
    opt::ConstantFold cf; (void)cf.run(*mod);
    opt::AlgebraicSimplify alg; (void)alg.run(*mod);
    opt::DCE dce; (void)dce.run(*mod);

    // 5) Print AST after
    const auto after = printer.print(*mod);
    if (it != exp.end()) {
      assert_contains_all(after, it->second.afterContains, "after");
    } else {
      ASSERT_NE(after.find("Module"), std::string::npos);
      ASSERT_NE(after.find("FunctionDef"), std::string::npos);
    }
  }
}
