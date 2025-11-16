#pragma once

#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct EllipsisLiteral final : Expr, Acceptable<EllipsisLiteral, NodeKind::EllipsisLiteral> {
  EllipsisLiteral() : Expr(NodeKind::EllipsisLiteral) {}
};

} // namespace pycc::ast
