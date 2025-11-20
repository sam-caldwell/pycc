/***
 * Name: pycc::opt::LICM
 * Purpose: Loop-invariant code motion for simple invariant assignments inside while loops.
 */
#pragma once

#include "optimizer/Pass.h"

namespace pycc { namespace ast { struct Module; } }

namespace pycc::opt {

class LICM : public Pass {
 public:
  // Returns number of statements hoisted.
  std::size_t run(ast::Module& module);
};

} // namespace pycc::opt

