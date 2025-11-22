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
        std::optional<TypeKind> annotatedType{};
        void setType(TypeKind t) { annotatedType = t; }
        std::optional<TypeKind> type() const { return annotatedType; }
        std::optional<std::string> canonicalKey{};
        void setCanonicalKey(std::string k) { canonicalKey = std::move(k); }
        std::optional<std::string> canonical() const { return canonicalKey; }
    };
} // namespace pycc::ast
