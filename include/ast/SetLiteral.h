#pragma once

#include <memory>
#include <vector>

#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct SetLiteral final : Expr, Acceptable<SetLiteral, NodeKind::SetLiteral> {
  std::vector<std::unique_ptr<Expr>> elements;
  SetLiteral() : Expr(NodeKind::SetLiteral) {}
};

} // namespace pycc::ast

