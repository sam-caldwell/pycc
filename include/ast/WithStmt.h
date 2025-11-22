/**
 * @file
 * @brief AST with statement declarations.
 */
#pragma once

#include <memory>
#include <vector>
#include "Stmt.h"
#include "ast/WithItem.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct WithStmt final : Stmt, Acceptable<WithStmt, NodeKind::WithStmt> {
        std::vector<std::unique_ptr<WithItem>> items;
        std::vector<std::unique_ptr<Stmt>> body;
        WithStmt() : Stmt(NodeKind::WithStmt) {}
    };
}
