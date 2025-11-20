/***
 * Name: pycc::opt::CSE
 * Purpose: Simple common subexpression elimination for trivially pure expression statements.
 */
#pragma once

#include "optimizer/Pass.h"

namespace pycc { namespace ast { struct Module; } }

namespace pycc::opt {

class CSE : public Pass {
 public:
  // Returns number of eliminated expressions.
  std::size_t run(ast::Module& module);
};

} // namespace pycc::opt

