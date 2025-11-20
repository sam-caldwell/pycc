/***
 * Name: pycc::opt::SSA
 * Purpose: SSA scaffolding for future lowering and analysis.
 */
#pragma once

#include <cstddef>

namespace pycc { namespace ast { struct Module; } }

namespace pycc::opt {

struct SSAStats { std::size_t values{0}; std::size_t instructions{0}; std::size_t blocks{0}; };

class SSA {
 public:
  // Analysis-only scaffold: walks functions and counts pure operation nodes as instructions/values.
  SSAStats analyze(const ast::Module& module);
};

} // namespace pycc::opt

