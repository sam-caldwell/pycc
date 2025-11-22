/**
 * @file
 * @brief AST named expression declarations.
 */
#pragma once

#include <memory>
#include <string>
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct NamedExpr final : Expr, Acceptable<NamedExpr, NodeKind::NamedExpr> {
  std::string target;                 // identifier being assigned (NAME)
  std::unique_ptr<Expr> value;        // expression value
  NamedExpr(std::string t, std::unique_ptr<Expr> v)
      : Expr(NodeKind::NamedExpr), target(std::move(t)), value(std::move(v)) {}
};

} // namespace pycc::ast
