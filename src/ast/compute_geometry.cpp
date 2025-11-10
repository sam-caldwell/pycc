/***
 * Name: pycc::ast::ComputeGeometry
 * Purpose: Compute AST geometry (node count and maximum depth) via DFS.
 * Inputs:
 *   - root: AST root node
 * Outputs:
 *   - out: populated geometry statistics
 * Theory of Operation: Performs a recursive traversal counting nodes and tracking depth.
 */
#include <algorithm>
#include <cstddef>

#include "pycc/ast/ast.h"

namespace pycc::ast {

static void DepthFirstAccumulate(const Node& node, std::size_t depth_value, ASTGeometry& out) {
  out.max_depth = std::max(depth_value, out.max_depth);
  ++out.node_count;
  for (const auto& child : node.children) {
    DepthFirstAccumulate(*child, depth_value + 1, out);
  }
}

auto ComputeGeometry(const Node& root, ASTGeometry& out) -> void {
  out = ASTGeometry{};
  constexpr std::size_t kInitialDepth = 1U;
  DepthFirstAccumulate(root, kInitialDepth, out);
}

}  // namespace pycc::ast
