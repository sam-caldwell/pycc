/***
 * Name: pycc::opt::AlgebraicSimplify
 * Purpose: Simple algebraic simplifications (x+0, x*1, etc.).
 */
#pragma once

#include "ast/Nodes.h"
#include "optimizer/Pass.h"
#include <unordered_map>

namespace pycc::opt {

class AlgebraicSimplify : public Pass {
 public:
  size_t run(ast::Module& m) override;
  const std::unordered_map<std::string, uint64_t>& stats() const { return stats_; }
 private:
  std::unordered_map<std::string, uint64_t> stats_{};
};

} // namespace pycc::opt

