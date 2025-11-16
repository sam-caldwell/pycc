#pragma once

#include <memory>
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct IfExpr final : Expr, Acceptable<IfExpr, NodeKind::IfExpr> {
  std::unique_ptr<Expr> test;
  std::unique_ptr<Expr> body;
  std::unique_ptr<Expr> orelse;
  IfExpr(std::unique_ptr<Expr> b, std::unique_ptr<Expr> t, std::unique_ptr<Expr> e)
      : Expr(NodeKind::IfExpr), test(std::move(t)), body(std::move(b)), orelse(std::move(e)) {}
};

} // namespace pycc::ast

