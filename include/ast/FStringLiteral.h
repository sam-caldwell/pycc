/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "ast/Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct FStringSegment {
  bool isExpr{false};
  std::string text; // when !isExpr
  std::unique_ptr<Expr> expr; // when isExpr; format ignored for now
};

struct FStringLiteral final : Expr, Acceptable<FStringLiteral, NodeKind::FStringLiteral> {
  std::vector<FStringSegment> parts;
  FStringLiteral() : Expr(NodeKind::FStringLiteral) {}
};

} // namespace pycc::ast
