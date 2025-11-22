/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <memory>
#include <vector>
#include "Expr.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct ComprehensionFor {
  std::unique_ptr<Expr> target;
  std::unique_ptr<Expr> iter;
  std::vector<std::unique_ptr<Expr>> ifs; // zero or more if guards
  bool isAsync{false};
};

struct ListComp final : Expr, Acceptable<ListComp, NodeKind::ListComp> {
  std::unique_ptr<Expr> elt;
  std::vector<ComprehensionFor> fors;
  ListComp() : Expr(NodeKind::ListComp) {}
};

struct SetComp final : Expr, Acceptable<SetComp, NodeKind::SetComp> {
  std::unique_ptr<Expr> elt;
  std::vector<ComprehensionFor> fors;
  SetComp() : Expr(NodeKind::SetComp) {}
};

struct DictComp final : Expr, Acceptable<DictComp, NodeKind::DictComp> {
  std::unique_ptr<Expr> key;
  std::unique_ptr<Expr> value;
  std::vector<ComprehensionFor> fors;
  DictComp() : Expr(NodeKind::DictComp) {}
};

struct GeneratorExpr final : Expr, Acceptable<GeneratorExpr, NodeKind::GeneratorExpr> {
  std::unique_ptr<Expr> elt;
  std::vector<ComprehensionFor> fors;
  GeneratorExpr() : Expr(NodeKind::GeneratorExpr) {}
};

} // namespace pycc::ast
