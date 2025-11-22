/**
 * @file
 * @brief AST base Node default accept implementation.
 */
/***
 * Name: pycc::ast::Node::accept
 * Purpose: Default dynamic dispatch via central switch for non-override nodes.
 */
#include "ast/Node.h"
#include "ast/Visitor.h"

namespace pycc::ast {

void Node::accept(VisitorBase& visitor) const {
    // Fallback to central dispatch
    dispatch(*this, visitor);
}

} // namespace pycc::ast
