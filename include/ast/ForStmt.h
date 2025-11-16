#pragma once

#include <memory>
#include <string>
#include "Expr.h"
#include "Stmt.h"
#include "ast/HasBodyPair.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct ForStmt final : Stmt, HasBodyPair<Stmt>, Acceptable<ForStmt, NodeKind::ForStmt> {
        std::unique_ptr<Expr> target;   // general target (name, tuple, list, attr, subscript)
        std::unique_ptr<Expr> iterable; // expression producing an iterable
        ForStmt(std::unique_ptr<Expr> t, std::unique_ptr<Expr> it)
            : Stmt(NodeKind::ForStmt), target(std::move(t)), iterable(std::move(it)) {}
    };
}
