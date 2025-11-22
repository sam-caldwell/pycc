/**
 * @file
 * @brief AST try/except declarations.
 */
#pragma once

#include <memory>
#include <vector>
#include "Stmt.h"
#include "ast/Acceptable.h"
#include "ast/ExceptHandler.h"

namespace pycc::ast {
    struct TryStmt final : Stmt, Acceptable<TryStmt, NodeKind::TryStmt> {
        std::vector<std::unique_ptr<Stmt>> body;
        std::vector<std::unique_ptr<ExceptHandler>> handlers;
        std::vector<std::unique_ptr<Stmt>> orelse;
        std::vector<std::unique_ptr<Stmt>> finalbody;
        TryStmt() : Stmt(NodeKind::TryStmt) {}
    };
}
