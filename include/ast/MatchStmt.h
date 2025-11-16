#pragma once

#include <memory>
#include <vector>
#include "ast/Stmt.h"
#include "ast/Expr.h"
#include "ast/Pattern.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct MatchCase final : Node, Acceptable<MatchCase, NodeKind::MatchCase> {
  std::unique_ptr<Pattern> pattern;
  std::unique_ptr<Expr> guard; // optional; null when absent
  std::vector<std::unique_ptr<Stmt>> body;
  MatchCase() : Node(NodeKind::MatchCase) {}
};

struct MatchStmt final : Stmt, Acceptable<MatchStmt, NodeKind::MatchStmt> {
  std::unique_ptr<Expr> subject;
  std::vector<std::unique_ptr<MatchCase>> cases;
  MatchStmt() : Stmt(NodeKind::MatchStmt) {}
};

} // namespace pycc::ast

