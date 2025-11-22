/**
 * @file
 * @brief AST raise statement declarations.
 */
#pragma once

#include <memory>
#include "Stmt.h"
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct RaiseStmt final : Stmt, Acceptable<RaiseStmt, NodeKind::RaiseStmt> {
  std::unique_ptr<Expr> exc;   // optional
  std::unique_ptr<Expr> cause; // optional after 'from'
  RaiseStmt() : Stmt(NodeKind::RaiseStmt) {}
};

} // namespace pycc::ast
