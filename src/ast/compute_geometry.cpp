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

namespace pycc {
namespace ast {

static void dfs(const Node& n, std::size_t depth, ASTGeometry& out) {
  if (depth > out.max_depth) out.max_depth = depth;
  ++out.node_count;
  for (const auto& ch : n.children) dfs(*ch, depth + 1, out);
}

void ComputeGeometry(const Node& root, ASTGeometry& out) {
  out = ASTGeometry{};
  dfs(root, 1, out);
}

}  // namespace ast
}  // namespace pycc

