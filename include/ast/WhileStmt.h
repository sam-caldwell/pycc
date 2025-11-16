#pragma once

#include <memory>
#include "Expr.h"
#include "Stmt.h"
#include "ast/HasBodyPair.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct WhileStmt final : Stmt, HasBodyPair<Stmt>, Acceptable<WhileStmt, NodeKind::WhileStmt> {
        std::unique_ptr<Expr> cond;
        explicit WhileStmt(std::unique_ptr<Expr> c) : Stmt(NodeKind::WhileStmt), cond(std::move(c)) {}
    };
}

