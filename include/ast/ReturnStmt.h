/**
 * @file
 * @brief AST return statement declarations.
 */
#pragma once

#include <memory>
#include "ast/Acceptable.h"
#include "ast/Expr.h"
#include "ast/Stmt.h"

namespace pycc::ast {
    struct ReturnStmt final : Stmt, Acceptable<ReturnStmt, NodeKind::ReturnStmt> {
        std::unique_ptr<Expr> value;
        explicit ReturnStmt(std::unique_ptr<Expr> v)
            : Stmt(NodeKind::ReturnStmt), value(std::move(v)) {}
    };

} // namespace pycc::ast
