/***
 * Name: pycc::ast::StringLiteral
 * Purpose: String literal node.
 */
#pragma once

#include <string>
#include "ast/Literal.h"

namespace pycc::ast {
    using StringLiteral = Literal<std::string, NodeKind::StringLiteral>;
}

