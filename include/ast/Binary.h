/**
 * @file
 * @brief AST declarations.
 */
#pragma once
#include <memory>

#include "BinaryOperator.h"
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {
    struct Binary final : Expr, Acceptable<Binary, NodeKind::BinaryExpr> {
        BinaryOperator op;
        std::unique_ptr<Expr> lhs;
        std::unique_ptr<Expr> rhs;

        Binary(const BinaryOperator o, std::unique_ptr<Expr> a, std::unique_ptr<Expr> b)
            : Expr(NodeKind::BinaryExpr), op(o), lhs(std::move(a)), rhs(std::move(b)) {
        }
    };
} // namespace pycc::ast
