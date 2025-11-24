/***
 * Name: pycc::opt::LoopUnroll
 * Purpose: Safely unroll simple for-range loops when a small, constant trip count
 *          and simple bodies make unrolling cheaper than loop overhead.
 */
#pragma once

#include <cstddef>

namespace pycc { namespace ast { struct Module; } }

namespace pycc::opt {

class LoopUnroll {
public:
  // Returns number of loops unrolled
  std::size_t run(ast::Module& module);
};

} // namespace pycc::opt

