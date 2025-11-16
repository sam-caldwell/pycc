#pragma once

#include <memory>
#include "ast/Stmt.h"
#include "ast/FunctionDef.h"
#include "ast/Acceptable.h"

namespace pycc::ast {

// Wrapper statement for function definitions inside class bodies or suites.
struct DefStmt final : Stmt, Acceptable<DefStmt, NodeKind::DefStmt> {
  std::unique_ptr<FunctionDef> func;
  explicit DefStmt(std::unique_ptr<FunctionDef> f)
      : Stmt(NodeKind::DefStmt), func(std::move(f)) {}
};

} // namespace pycc::ast

