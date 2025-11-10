/***
 * Name: pycc::ast (AST)
 * Purpose: Minimal AST with templated node wrapper to keep definitions DRY and
 *   standardize patterns across node kinds.
 * Inputs: Constructed AST nodes
 * Outputs: Geometry stats (node count, max depth)
 * Theory of Operation: Base Node stores kind and children. Templated NodeT<K,Payload>
 *   provides a uniform node with an optional payload. Helper templates standardize
 *   node creation and child attachment. Geometry computed via DFS.
 */
#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace pycc {
namespace ast {

enum class NodeKind { Module, FunctionDef, ReturnStmt, IntLiteral };

/***
 * Name: pycc::ast::Node
 * Purpose: Base node interface providing kind and children storage.
 * Inputs: kind on construction
 * Outputs: N/A
 * Theory of Operation: Holds common state for all node kinds.
 */
struct Node {
  explicit Node(NodeKind k) : kind(k) {}
  virtual ~Node() = default;
  NodeKind kind;
  std::vector<std::unique_ptr<Node>> children;
};

/***
 * Name: pycc::ast::NodeT
 * Purpose: Templated node wrapper parameterized by NodeKind and Payload type.
 * Inputs: Optional payload value
 * Outputs: Node instance storing payload and children
 * Theory of Operation: Keeps AST DRY by unifying node structure across kinds and
 *   allowing payloads where needed (e.g., literals, identifiers).
 */
template <NodeKind K, typename Payload = std::monostate>
struct NodeT : public Node {
  explicit NodeT(Payload p = Payload{}) : Node(K), payload(std::move(p)) {}
  Payload payload;
};

// Concrete aliases for current MVP
using Module = NodeT<NodeKind::Module>;
using FunctionDef = NodeT<NodeKind::FunctionDef>;
using ReturnStmt = NodeT<NodeKind::ReturnStmt>;
using IntLiteral = NodeT<NodeKind::IntLiteral, int>;

/***
 * Name: pycc::ast::make_node
 * Purpose: Template factory for AST nodes.
 * Inputs: Constructor args for NodeType
 * Outputs: std::unique_ptr<NodeType>
 * Theory of Operation: Forwards args to NodeType constructor.
 */
template <typename NodeType, typename... Args>
std::unique_ptr<NodeType> make_node(Args&&... args) {
  return std::make_unique<NodeType>(std::forward<Args>(args)...);
}

/***
 * Name: pycc::ast::add_child
 * Purpose: Standardize child attachment for AST nodes.
 * Inputs: parent node, child unique_ptr
 * Outputs: Parent now owns child
 * Theory of Operation: Moves child pointer into parent's children vector.
 */
template <typename ParentNode, typename ChildNode>
void add_child(ParentNode& parent, std::unique_ptr<ChildNode> child) {
  parent.children.emplace_back(std::move(child));
}

struct ASTGeometry { std::size_t node_count{0}; std::size_t max_depth{0}; };

/*** ComputeGeometry: Populate node_count and max_depth for a given root. */
void ComputeGeometry(const Node& root, ASTGeometry& out);

}  // namespace ast
}  // namespace pycc
