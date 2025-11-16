#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Expr.h"
#include "Stmt.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct ExceptHandler final : Node, Acceptable<ExceptHandler, NodeKind::ExceptHandler> {
        std::unique_ptr<Expr> type; // may be null
        std::string name;           // optional name (empty if none)
        std::vector<std::unique_ptr<Stmt>> body;
        ExceptHandler() : Node(NodeKind::ExceptHandler) {}
    };
}

