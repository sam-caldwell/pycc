/***
 * Name: ScopedLocalsAssigned
 * Purpose: Manage the current function's locals-assigned set for name resolution.
 */
#include "sema/detail/LocalsAssigned.h"

namespace pycc::sema::detail {
    const std::unordered_set<std::string> *g_locals_assigned = nullptr; // NOLINT

    ScopedLocalsAssigned::ScopedLocalsAssigned(const std::unordered_set<std::string> *cur)
        : prev(g_locals_assigned) { g_locals_assigned = cur; }

    ScopedLocalsAssigned::~ScopedLocalsAssigned() { g_locals_assigned = prev; }
} // namespace pycc::sema::detail
