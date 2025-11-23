/***
 * Name: pycc::opt::Pass
 * Purpose: Base interface for optimizer passes over the AST.
 * Inputs:
 *   - ast::Module (mutable)
 * Outputs:
 *   - Count of transformations performed.
 */
#pragma once

#include <cstddef>
#include "ast/Nodes.h"

namespace pycc::opt {
    class Pass {
    public:
        virtual ~Pass() = default;

        virtual size_t run(ast::Module &m) = 0;
    };
} // namespace pycc::opt
