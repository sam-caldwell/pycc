/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include "Node.h"
#include "ast/TypeKind.h"
#include <optional>
#include <string>

namespace pycc::ast {
    struct Expr : Node {
        using Node::Node;
        // These are mutable so that sema can annotate types and canonical forms
        // while traversing const AST nodes without casting away const.
        mutable std::optional<TypeKind> annotatedType{};
        void setType(TypeKind t) const { annotatedType = t; }
        std::optional<TypeKind> type() const { return annotatedType; }
        mutable std::optional<std::string> canonicalKey{};
        void setCanonicalKey(std::string k) const { canonicalKey = std::move(k); }
        std::optional<std::string> canonical() const { return canonicalKey; }
    };
} // namespace pycc::ast
