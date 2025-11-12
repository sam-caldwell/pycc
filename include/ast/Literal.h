#pragma once

#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

template <typename T, NodeKind K>
struct Literal final : Expr, Acceptable<Literal<T, K>, K> {
    T value;
    explicit Literal(const T v) : Expr(K), value(v) {}
};

} // namespace pycc::ast
