#pragma once

#include <memory>

#include "Expr.h"
#include "Stmt.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

    struct AssignStmt final : Stmt, Acceptable<AssignStmt, NodeKind::AssignStmt> {
        std::string target;
        std::unique_ptr<Expr> value;
        AssignStmt(std::string t, std::unique_ptr<Expr> v)
            : Stmt(NodeKind::AssignStmt), target(std::move(t)), value(std::move(v)) {}
    };
} // namespace pycc::ast
