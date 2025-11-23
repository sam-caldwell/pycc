/***
 * Name: pycc::opt::LICM
 * Purpose: Loop-invariant code motion for simple invariant assignments inside while loops.
 */
#pragma once

#include "optimizer/Pass.h"

namespace pycc {
    namespace ast {
        struct Module;
    }
}

namespace pycc::opt {
    class LICM final : public Pass {
    public:
        // Returns the number of statements hoisted.
        std::size_t run(ast::Module &module) override;
    };
} // namespace pycc::opt
