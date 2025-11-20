/***
 * Name: pycc::opt::LocalProp
 * Purpose: Intra-block constant and copy propagation with conservative semantics.
 * Inputs:
 *   - ast::Module (mutable)
 * Outputs:
 *   - Number of propagated/rewritten sites.
 * Notes:
 *   - Propagates literal constants and simple aliases (x = y) within straight-line code.
 *   - Resets propagation environment across control-flow boundaries (if/loops/try/match/class/def).
 *   - Does not introduce temporaries or reorder side-effecting code.
 */
#pragma once

#include <cstddef>
#include "optimizer/Pass.h"
#include "ast/Nodes.h"

namespace pycc::opt {

class LocalProp : public Pass {
 public:
  std::size_t run(ast::Module& m) override;
};

} // namespace pycc::opt

