#pragma once

#include <memory>
#include <vector>
#include "Expr.h"
#include "Stmt.h"
#include "ast/HasBodyPair.h"
#include "ast/Acceptable.h"


namespace pycc::ast {
    struct IfStmt final : Stmt, HasBodyPair<Stmt>, Acceptable<IfStmt, NodeKind::IfStmt> {
        std::unique_ptr<Expr> cond;
        explicit IfStmt(std::unique_ptr<Expr> c) : Stmt(NodeKind::IfStmt), cond(std::move(c)) {}
    };
} // namespace pycc::ast
