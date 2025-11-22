/**
 * @file
 * @brief AST declarations.
 */
/***
 * Name: pycc::ast::ExprStmt
 * Purpose: Represent a standalone expression as a statement.
 * Theory of Operation:
 *   Wraps an Expr in a Stmt for cases like expression statements.
 */
#pragma once

#include <memory>
#include "ast/Stmt.h"
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct ExprStmt final : Stmt, Acceptable<ExprStmt, NodeKind::ExprStmt> {
  std::unique_ptr<Expr> value;
  explicit ExprStmt(std::unique_ptr<Expr> v) : Stmt(NodeKind::ExprStmt), value(std::move(v)) {}
};

} // namespace pycc::ast
