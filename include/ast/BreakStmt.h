/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include "Stmt.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct BreakStmt final : Stmt, Acceptable<BreakStmt, NodeKind::BreakStmt> {
        BreakStmt() : Stmt(NodeKind::BreakStmt) {}
    };
}
