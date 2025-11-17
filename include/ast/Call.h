#pragma once
#include <memory>
#include <vector>

#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct KeywordArg { std::string name; std::unique_ptr<Expr> value; };

    struct Call final : Expr, Acceptable<Call, NodeKind::Call> {
        std::unique_ptr<Expr> callee; // typically Name
        std::vector<std::unique_ptr<Expr>> args;      // positional
        std::vector<KeywordArg> keywords;             // named args
        std::vector<std::unique_ptr<Expr>> starArgs;  // *expr
        std::vector<std::unique_ptr<Expr>> kwStarArgs;// **expr
        explicit Call(std::unique_ptr<Expr> c) : Expr(NodeKind::Call), callee(std::move(c)) {}
    };

} // namespace pycc::ast
