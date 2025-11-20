/***
 * Name: pycc::opt::SSABuilder
 * Purpose: Build a minimal SSA-like scaffold: split basic blocks at control constructs and
 *          assign value IDs to pure expressions within blocks. This is not a full SSA with phi.
 */
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace pycc { namespace ast { struct FunctionDef; struct Stmt; struct Expr; } }

namespace pycc::opt {

struct SSABlock {
  int id{0};
  std::vector<ast::Stmt*> stmts; // raw pointers into AST for scaffolding
  std::vector<int> succ;         // successor block IDs
  std::vector<int> pred;         // predecessor block IDs
  // SSA scaffolding
  std::unordered_set<std::string> defs; // names assigned in this block
  struct SSAPhi { std::string var; std::vector<int> incomings; };
  std::vector<SSAPhi> phis; // phi placeholders for join blocks
};

struct SSAFunction {
  const ast::FunctionDef* fn{nullptr};
  std::vector<SSABlock> blocks;
  std::unordered_map<const ast::Stmt*, int> blockOf; // stmt -> block id
};

class SSABuilder {
 public:
  // Build blocks and CFG for a single function (scaffold; no phis).
  SSAFunction build(ast::FunctionDef& fn);

  struct DomTree {
    // idom[i] = immediate dominator block id of i, or -1 for entry/unreachable
    std::vector<int> idom;
    std::vector<std::vector<int>> children; // dominator tree adjacency
  };

  // Compute a simple dominator tree over the scaffold CFG.
  DomTree computeDominators(const SSAFunction& fn) const;
};

} // namespace pycc::opt
