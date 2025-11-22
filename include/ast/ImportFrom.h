/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <string>
#include <vector>
#include "ast/Stmt.h"
#include "ast/Alias.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct ImportFrom final : Stmt, Acceptable<ImportFrom, NodeKind::ImportFrom> {
        std::string module; // empty for relative-only
        int level{0};       // number of leading dots
        std::vector<Alias> names;
        ImportFrom() : Stmt(NodeKind::ImportFrom) {}
    };
}
