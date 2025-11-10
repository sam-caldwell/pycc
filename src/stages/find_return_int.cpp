/***
 * Name: pycc::stages::detail::FindReturnIntLiteral
 * Purpose: Walk AST and return IntLiteral under main's return statement.
 * Inputs: AST root
 * Outputs: Optional integer value
 * Theory of Operation: Scan Module -> FunctionDef -> ReturnStmt -> IntLiteral.
 */
#include "pycc/stages/detail/ir_helpers.h"
#include "pycc/ast/ast.h"

#include <optional>

namespace pycc {
namespace stages {
namespace detail {

auto FindReturnIntLiteral(const ast::Node& root) -> std::optional<int> {  // NOLINT(readability-function-cognitive-complexity,readability-function-size)
  for (const auto& function_node : root.children) {
    if (function_node->kind != ast::NodeKind::FunctionDef) {
      continue;
    }
    for (const auto& stmt_node : function_node->children) {
      const bool is_return = (stmt_node->kind == ast::NodeKind::ReturnStmt);
      const bool has_int = (!stmt_node->children.empty() &&
                            stmt_node->children[0]->kind == ast::NodeKind::IntLiteral);
      if (is_return && has_int) {
        if (const auto* literal = dynamic_cast<const ast::IntLiteral*>(stmt_node->children[0].get()); literal != nullptr) {
          return literal->payload;
        }
      }
    }
  }
  return std::nullopt;
}

}  // namespace detail
}  // namespace stages
}  // namespace pycc
