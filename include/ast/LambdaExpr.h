/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct LambdaExpr final : Expr, Acceptable<LambdaExpr, NodeKind::LambdaExpr> {
  std::vector<std::string> params;
  std::unique_ptr<Expr> body;
  LambdaExpr() : Expr(NodeKind::LambdaExpr) {}
};

} // namespace pycc::ast
