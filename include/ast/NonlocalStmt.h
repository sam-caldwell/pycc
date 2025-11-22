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

struct NonlocalStmt final : Stmt, Acceptable<NonlocalStmt, NodeKind::NonlocalStmt> {
  std::vector<std::string> names;
  NonlocalStmt() : Stmt(NodeKind::NonlocalStmt) {}
};

} // namespace pycc::ast
