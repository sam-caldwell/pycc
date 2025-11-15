/***
 * Name: test_geometry
 * Purpose: Cover AST geometry summary and DepthScope nesting.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/GeometrySummary.h"

using namespace pycc;

static std::unique_ptr<ast::Module> parseSrcGeom(const char* src) {
  lex::Lexer L; L.pushString(src, "geo.py");
  parse::Parser P(L);
  return P.parseModule();
}

TEST(Geometry, NestedDepthIncreases) {
  const char* shallow =
      "def main() -> int:\n"
      "  return 1 + 2\n";
  const char* deep =
      "def main() -> int:\n"
      "  return 1 + (2 * (3 + 4))\n";
  auto modS = parseSrcGeom(shallow);
  auto modD = parseSrcGeom(deep);
  const auto gS = ast::ComputeGeometry(*modS);
  const auto gD = ast::ComputeGeometry(*modD);
  EXPECT_GT(gS.nodes, 0u);
  EXPECT_GT(gD.nodes, gS.nodes);
  EXPECT_GT(gD.maxDepth, gS.maxDepth);
}

