#pragma once

#include "Stmt.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct ContinueStmt final : Stmt, Acceptable<ContinueStmt, NodeKind::ContinueStmt> {
        ContinueStmt() : Stmt(NodeKind::ContinueStmt) {}
    };
}

