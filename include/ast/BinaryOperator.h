#pragma once

namespace pycc::ast {

enum class BinaryOperator {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    FloorDiv,
    Pow,
    LShift,
    RShift,
    BitAnd,
    BitOr,
    BitXor,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    Is,
    IsNot,
    In,
    NotIn,
    And,
    Or
};

} // namespace pycc::ast
