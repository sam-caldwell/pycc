/***
 * Name: pycc::opt::SSAGVN
 * Purpose: Minimal GVN over SSA blocks: eliminate identical pure subexpressions within blocks
 *          and across straight-line single-predecessor chains.
 */
#pragma once

#include <cstddef>

namespace pycc { namespace ast { struct Module; } }

namespace pycc::opt {

class SSAGVN {
 public:
  // Returns number of subexpressions eliminated via temp reuse.
  std::size_t run(ast::Module& module);
};

} // namespace pycc::opt

