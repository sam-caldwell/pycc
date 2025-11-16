#pragma once

#include <memory>
#include <string>
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct WithItem final : Node, Acceptable<WithItem, NodeKind::WithItem> {
        std::unique_ptr<Expr> context;
        std::string asName; // empty if none
        WithItem() : Node(NodeKind::WithItem) {}
    };
}

