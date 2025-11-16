#pragma once

#include <memory>
#include "ast/Expr.h"
#include "ast/Acceptable.h"
#include "ast/ExprContext.h"

namespace pycc::ast {

struct Subscript final : Expr, Acceptable<Subscript, NodeKind::Subscript> {
  std::unique_ptr<Expr> value;
  std::unique_ptr<Expr> slice;
  ExprContext ctx{ExprContext::Load};
  Subscript(std::unique_ptr<Expr> v, std::unique_ptr<Expr> s)
      : Expr(NodeKind::Subscript), value(std::move(v)), slice(std::move(s)) {}
};

} // namespace pycc::ast

