/**
 * @file
 * @brief AST yield/await expression declarations.
 */
#pragma once

#include <memory>
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct YieldExpr final : Expr, Acceptable<YieldExpr, NodeKind::YieldExpr> {
  bool isFrom{false};
  std::unique_ptr<Expr> value; // optional when not 'from'
  YieldExpr() : Expr(NodeKind::YieldExpr) {}
};

} // namespace pycc::ast
