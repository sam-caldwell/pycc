/***
 * Name: pycc::opt::RangeAnalysis
 * Purpose: Simple integer range analysis (analysis only) from literal assignments.
 */
#pragma once

#include "optimizer/Pass.h"
#include <unordered_map>
#include <string>

namespace pycc { namespace ast { struct Module; } }

namespace pycc::opt {

class RangeAnalysis : public Pass {
 public:
  struct Range { long long min; long long max; };
  std::unordered_map<std::string, Range> analyze(const ast::Module& module);
  size_t run(ast::Module& m) override; // analysis-only: no transformations
};

} // namespace pycc::opt
