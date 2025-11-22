/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <memory>
#include <vector>
#include "Expr.h"
#include "ast/BinaryOperator.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct Compare final : Expr, Acceptable<Compare, NodeKind::Compare> {
  std::unique_ptr<Expr> left;
  std::vector<BinaryOperator> ops;
  std::vector<std::unique_ptr<Expr>> comparators; // length equals ops.size()
  Compare() : Expr(NodeKind::Compare) {}
};

} // namespace pycc::ast
