/***
 * Name: pycc::opt::SimplifyScopes
 * Purpose: Scope-level simplifications that do not alter language behavior:
 *   - Remove Pass statements
 *   - Merge identical return branches in simple if-statements
 */
#pragma once

#include "ast/Nodes.h"
#include <cstddef>

namespace pycc::opt {

class SimplifyScopes {
 public:
  // Returns number of changes applied
  std::size_t run(ast::Module& module);
};

} // namespace pycc::opt

