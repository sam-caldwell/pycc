#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct DictLiteral final : Expr, Acceptable<DictLiteral, NodeKind::DictLiteral> {
  // sequence of key:value pairs
  std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> items;
  DictLiteral() : Expr(NodeKind::DictLiteral) {}
};

} // namespace pycc::ast

