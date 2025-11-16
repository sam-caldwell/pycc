#pragma once

#include <memory>
#include "Stmt.h"
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct AssertStmt final : Stmt, Acceptable<AssertStmt, NodeKind::AssertStmt> {
  std::unique_ptr<Expr> test;
  std::unique_ptr<Expr> msg; // optional
  AssertStmt() : Stmt(NodeKind::AssertStmt) {}
};

} // namespace pycc::ast

