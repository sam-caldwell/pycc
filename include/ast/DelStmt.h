#pragma once

#include <memory>
#include <vector>
#include "ast/Stmt.h"
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct DelStmt final : Stmt, Acceptable<DelStmt, NodeKind::DelStmt> {
        std::vector<std::unique_ptr<Expr>> targets;
        DelStmt() : Stmt(NodeKind::DelStmt) {}
    };
}

