/***
 * Name: pycc::opt::SimplifyCFG
 * Purpose: Simplify control flow by pruning constant conditions.
 */
#pragma once

#include <unordered_map>
#include "ast/Nodes.h"
#include "optimizer/Pass.h"

namespace pycc::opt {
    class SimplifyCFG final : public Pass {
    public:
        size_t run(ast::Module &m) override;

        const std::unordered_map<std::string, uint64_t> &stats() const { return stats_; }

    private:
        std::unordered_map<std::string, uint64_t> stats_{};
    };
} // namespace pycc::opt
