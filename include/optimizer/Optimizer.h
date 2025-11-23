/***
 * Name: pycc::opt::Optimizer
 * Purpose: Provide a visitor-based optimizer scaffold for AST passes.
 * Inputs:
 *   - ast::Module
 * Outputs:
 *   - Stats about traversals and (future) transformations.
 * Theory of Operation:
 *   Uses visitor patterns to walk the AST and collect statistics. Future
 *   implementations can add expression/statement rewriters for constant
 *   folding and algebraic simplification.
 */
#pragma once

#include <cstddef>
#include "ast/Nodes.h"

namespace pycc::opt {
    struct Stats {
        size_t nodesVisited{0};
        size_t exprsVisited{0};
        size_t stmtsVisited{0};
    };

    class Optimizer {
    public:
        Stats analyze(const ast::Module &m) const;
    };
} // namespace pycc::opt
