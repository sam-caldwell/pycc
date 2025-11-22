/**
 * @file
 * @brief AST unary operator enumeration.
 */
#pragma once

namespace pycc::ast {

enum class UnaryOperator {
    Neg,
    Not,
    BitNot
};

} // namespace pycc::ast
