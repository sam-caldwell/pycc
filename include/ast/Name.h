/**
 * @file
 * @brief AST name node declarations.
 */
#pragma once
#include "Expr.h"
#include "ast/ExprContext.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

    struct Name final : Expr, Acceptable<Name, NodeKind::Name> {
        std::string id;
        ExprContext ctx{ExprContext::Load};
        explicit Name(std::string s) : Expr(NodeKind::Name), id(std::move(s)) {}
    };

} // namespace pycc::ast
