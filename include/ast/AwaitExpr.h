/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <memory>
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct AwaitExpr final : Expr, Acceptable<AwaitExpr, NodeKind::AwaitExpr> {
  std::unique_ptr<Expr> value;
  AwaitExpr() : Expr(NodeKind::AwaitExpr) {}
};

} // namespace pycc::ast
