/***
 * Name: pycc::ast::ObjectLiteral
 * Purpose: Represent a simple fixed-field object literal.
 */
#pragma once

#include <memory>
#include <vector>
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct ObjectLiteral final : Expr, Acceptable<ObjectLiteral, NodeKind::ObjectLiteral> {
  std::vector<std::unique_ptr<Expr>> fields; // positional fields
  ObjectLiteral() : Expr(NodeKind::ObjectLiteral) {}
};

} // namespace pycc::ast

