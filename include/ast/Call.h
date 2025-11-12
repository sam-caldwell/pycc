#pragma once
#include <memory>
#include <vector>

#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct Call final : Expr, Acceptable<Call, NodeKind::Call> {
        std::unique_ptr<Expr> callee; // typically Name
        std::vector<std::unique_ptr<Expr>> args;
        explicit Call(std::unique_ptr<Expr> c) : Expr(NodeKind::Call), callee(std::move(c)) {}
    };

} // namespace pycc::ast
