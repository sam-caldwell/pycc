#pragma once

#include <string>
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct Alias final : Node, Acceptable<Alias, NodeKind::Alias> {
        std::string name;
        std::string asname; // empty if none
        Alias() : Node(NodeKind::Alias) {}
        Alias(std::string n, std::string a) : Node(NodeKind::Alias), name(std::move(n)), asname(std::move(a)) {}
    };
}

