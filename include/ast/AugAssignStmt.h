#pragma once

#include <memory>

#include "Stmt.h"
#include "Expr.h"
#include "ast/Acceptable.h"
#include "ast/BinaryOperator.h"

namespace pycc::ast {

struct AugAssignStmt final : Stmt, Acceptable<AugAssignStmt, NodeKind::AugAssignStmt> {
  std::unique_ptr<Expr> target;
  ast::BinaryOperator op; // Add/Sub/Mul/...
  std::unique_ptr<Expr> value;
  AugAssignStmt(std::unique_ptr<Expr> t, ast::BinaryOperator o, std::unique_ptr<Expr> v)
      : Stmt(NodeKind::AugAssignStmt), target(std::move(t)), op(o), value(std::move(v)) {}
};

} // namespace pycc::ast

