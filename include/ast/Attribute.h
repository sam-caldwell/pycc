/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <memory>
#include <string>
#include "ast/Expr.h"
#include "ast/Acceptable.h"
#include "ast/ExprContext.h"

namespace pycc::ast {

struct Attribute final : Expr, Acceptable<Attribute, NodeKind::Attribute> {
  std::unique_ptr<Expr> value;
  std::string attr;
  ExprContext ctx{ExprContext::Load};
  Attribute(std::unique_ptr<Expr> v, std::string a)
      : Expr(NodeKind::Attribute), value(std::move(v)), attr(std::move(a)) {}
};

} // namespace pycc::ast
