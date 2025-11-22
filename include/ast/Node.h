/**
 * @file
 * @brief AST base node declarations.
 */
#pragma once

#include "NodeKind.h"
#include <string>

namespace pycc::ast {

    struct VisitorBase; // fwd

    struct Node {
        NodeKind kind;
        explicit Node(const NodeKind k) : kind(k) {}
        virtual ~Node() = default;

        // Polymorphic dispatch entrypoint (default implemented out-of-line)
        virtual void accept(VisitorBase& v) const;

        int line{0};
        int col{0};
        std::string file{};
    };

} // namespace pycc::ast
