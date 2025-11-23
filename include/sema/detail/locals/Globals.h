/***
 * Name: g_locals_assigned
 * Purpose: Pointer to current function's set of locally-assigned names.
 */
#pragma once

#include <string>
#include <unordered_set>

namespace pycc::sema::detail {
extern const std::unordered_set<std::string>* g_locals_assigned;
}

