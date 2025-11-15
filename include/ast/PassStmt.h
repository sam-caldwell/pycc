#pragma once

#include "Stmt.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct PassStmt final : Stmt, Acceptable<PassStmt, NodeKind::PassStmt> {
        PassStmt() : Stmt(NodeKind::PassStmt) {}
    };
}

