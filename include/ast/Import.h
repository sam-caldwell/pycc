#pragma once

#include <vector>
#include <memory>
#include "ast/Stmt.h"
#include "ast/Alias.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct Import final : Stmt, Acceptable<Import, NodeKind::Import> {
        std::vector<Alias> names;
        Import() : Stmt(NodeKind::Import) {}
    };
}

