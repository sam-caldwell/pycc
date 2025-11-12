#pragma once

#include <cstdint>
#include "ast/Module.h"


namespace pycc::ast {
    // Compute a simple geometry summary for a module
    struct GeometrySummary {
        uint64_t nodes{0};
        uint64_t maxDepth{0};
    };

    GeometrySummary ComputeGeometry(const Module& m);

} // namespace pycc::ast