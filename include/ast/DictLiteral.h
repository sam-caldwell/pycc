/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct DictLiteral final : Expr, Acceptable<DictLiteral, NodeKind::DictLiteral> {
  // key:value pairs
  std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> items;
  // '**expr' unpack entries
  std::vector<std::unique_ptr<Expr>> unpacks;
  DictLiteral() : Expr(NodeKind::DictLiteral) {}
};

} // namespace pycc::ast
