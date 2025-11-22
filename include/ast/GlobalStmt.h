/**
 * @file
 * @brief AST declarations.
 */
#pragma once

#include <string>
#include <vector>
#include "Stmt.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

struct GlobalStmt final : Stmt, Acceptable<GlobalStmt, NodeKind::GlobalStmt> {
  std::vector<std::string> names;
  GlobalStmt() : Stmt(NodeKind::GlobalStmt) {}
};

} // namespace pycc::ast
