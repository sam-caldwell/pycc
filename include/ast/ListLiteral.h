/**
 * @file
 * @brief AST list literal declarations.
 */
/***
 * Name: pycc::ast::ListLiteral
 * Purpose: Represent a list literal (read-only ops in M2).
 */
#pragma once

#include <memory>
#include <vector>
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct ListLiteral final : Expr, Acceptable<ListLiteral, NodeKind::ListLiteral> {
  std::vector<std::unique_ptr<Expr>> elements;
  ListLiteral() : Expr(NodeKind::ListLiteral) {}
};

} // namespace pycc::ast
