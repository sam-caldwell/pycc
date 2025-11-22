/***
 * Name: pycc::opt::GVN
 * Purpose: Global value numbering (analysis-only scaffold) to identify redundant pure expressions.
 */
#pragma once

#include "optimizer/Pass.h"
#include <unordered_map>
#include <string>

namespace pycc { namespace ast { struct Module; struct FunctionDef; struct Expr; } }

namespace pycc::opt {

class GVN : public Pass {
 public:
  struct Result { std::size_t classes{0}; std::size_t expressions{0}; };
  Result analyze(const ast::Module& module);
  size_t run(ast::Module& m) override; // analysis-only: no transformations
};

} // namespace pycc::opt
