/***
 * Name: pycc::opt::ConstantFold
 * Purpose: Visitor-based constant folding for simple expressions.
 * Inputs:
 *   - ast::Module (mutable)
 * Outputs:
 *   - Number of fold transformations applied.
 * Theory of Operation:
 *   Recursively folds Unary/Binary expressions with literal operands and
 *   simplifies arithmetic where possible. Conservative; does not change
 *   control-flow (no branch pruning).
 */
#pragma once

#include <cstddef>
#include "ast/Nodes.h"
#include "optimizer/Pass.h"
#include <unordered_map>

namespace pycc::opt {

class ConstantFold : public Pass {
 public:
  size_t run(ast::Module& m) override;
  const std::unordered_map<std::string, uint64_t>& stats() const { return stats_; }
 private:
  std::unordered_map<std::string, uint64_t> stats_{};
};

} // namespace pycc::opt
