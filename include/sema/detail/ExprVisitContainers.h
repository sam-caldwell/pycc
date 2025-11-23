/***
 * @file
 * @brief Helpers for ExpressionTyper container visits (tuple/list/object).
 */
#pragma once

#include "ast/TypeKind.h"
#include <cstdint>
#include <functional>

namespace pycc::ast {
    struct Expr;
    struct TupleLiteral;
    struct ListLiteral;
    struct ObjectLiteral;
}

namespace pycc::sema::expr {
    /***
     * @brief Visit a tuple literal: visit children, set type and canonical.
     * @param tup Tuple node to process
     * @param out On success, set to Tuple kind
     * @param outSet On success, set to mask for Tuple
     * @param visitChild Callback to visit child expressions; must return true on success.
     * @return true on success, false if a child visit failed.
     */
    bool handleTupleLiteral(const ast::TupleLiteral &tup, ast::TypeKind &out, uint32_t &outSet,
                            const std::function<bool(const ast::Expr *)> &visitChild);

    /***
     * @brief Visit a list literal: visit children, set type and canonical.
     */
    bool handleListLiteral(const ast::ListLiteral &lst, ast::TypeKind &out, uint32_t &outSet,
                           const std::function<bool(const ast::Expr *)> &visitChild);

    /***
     * @brief Visit an object literal: visit fields; treat as opaque, set canonical.
     */
    bool handleObjectLiteral(const ast::ObjectLiteral &obj, ast::TypeKind &out, uint32_t &outSet,
                             const std::function<bool(const ast::Expr *)> &visitChild);
} // namespace pycc::sema::expr
