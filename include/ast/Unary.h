#pragma once
#include <memory>

#include "Expr.h"
#include "UnaryOperator.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct Unary final : Expr, Acceptable<Unary, NodeKind::UnaryExpr> {
        UnaryOperator op;
        std::unique_ptr<Expr> operand;
        Unary(const UnaryOperator o, std::unique_ptr<Expr> v)
            : Expr(NodeKind::UnaryExpr), op(o), operand(std::move(v)) {}
    };
} // namespace pycc::ast
