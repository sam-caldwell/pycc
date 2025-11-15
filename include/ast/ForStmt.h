#pragma once

#include <memory>
#include <string>
#include "Expr.h"
#include "Stmt.h"
#include "ast/HasBody.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct ForStmt final : Stmt, HasBody<Stmt>, Acceptable<ForStmt, NodeKind::ForStmt> {
        std::string var;
        std::unique_ptr<Expr> iterable;
        ForStmt(std::string v, std::unique_ptr<Expr> it) : Stmt(NodeKind::ForStmt), var(std::move(v)), iterable(std::move(it)) {}
    };
} // namespace pycc::ast

