/***
 * Name: pycc::ast::ComputeGeometry
 * Purpose: Compute AST geometry (node count and maximum depth) via DFS.
 * Inputs:
 *   - root: AST root node
 * Outputs:
 *   - out: populated geometry statistics
 * Theory of Operation: Performs a recursive traversal counting nodes and tracking depth.
 */
#include "pycc/ast/ast.h"

namespace pycc::ast {

static void DepthFirstAccumulate(const Node& node, std::size_t depth_value, ASTGeometry& out) {
  if (depth_value > out.max_depth) {
    out.max_depth = depth_value;
  }
  ++out.node_count;
  for (const auto& child : node.children) {
    DepthFirstAccumulate(*child, depth_value + 1, out);
  }
}

auto ComputeGeometry(const Node& root, ASTGeometry& out) -> void {
  out = ASTGeometry{};
  DepthFirstAccumulate(root, 1, out);
}

}  // namespace pycc::ast
