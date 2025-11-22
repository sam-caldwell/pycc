/**
 * @file
 * @brief AST statement base declarations.
 */
#pragma once

#include "ast/Node.h"

namespace pycc::ast {
    struct Stmt : Node {
        using Node::Node;
    };
}
