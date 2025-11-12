#pragma once
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

    struct Name final : Expr, Acceptable<Name, NodeKind::Name> {
        std::string id;
        explicit Name(std::string s) : Expr(NodeKind::Name), id(std::move(s)) {}
    };

} // namespace pycc::ast
