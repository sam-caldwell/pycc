/**
 * @file
 * @brief AST tuple literal declarations.
 */
/***
 * Name: pycc::ast::TupleLiteral
 * Purpose: Represent an immutable tuple literal of expressions.
 */
#pragma once

#include <memory>
#include <vector>
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct TupleLiteral final : Expr, Acceptable<TupleLiteral, NodeKind::TupleLiteral> {
  std::vector<std::unique_ptr<Expr>> elements;
  TupleLiteral() : Expr(NodeKind::TupleLiteral) {}
};

} // namespace pycc::ast
