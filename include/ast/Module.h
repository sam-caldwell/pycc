#pragma once

#include <memory>
#include <vector>
#include "ast/FunctionDef.h"
#include "ast/Node.h"
#include "ast/HasChildren.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct Module final : Node, HasChildren<FunctionDef>, Acceptable<Module, NodeKind::Module> {
        // Backward-compatible alias accessor for existing code/tests
        std::vector<std::unique_ptr<FunctionDef>>& functions = this->children;
        Module() : Node(NodeKind::Module) {}
    };
} // namespace pycc::ast
