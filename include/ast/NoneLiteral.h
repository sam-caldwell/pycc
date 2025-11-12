/***
 * Name: pycc::ast::NoneLiteral
 * Purpose: Represent the Python None literal.
 */
#pragma once

#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct NoneLiteral final : Expr, Acceptable<NoneLiteral, NodeKind::NoneLiteral> {
  NoneLiteral() : Expr(NodeKind::NoneLiteral) {}
};

}

