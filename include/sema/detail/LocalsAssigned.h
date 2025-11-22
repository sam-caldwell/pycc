/***
 * @file
 * @brief Helper to scope the set of locally-assigned names for ExpressionTyper.
 */
#pragma once

#include <string>
#include <unordered_set>

namespace pycc::sema::detail {

/*** @brief Pointer to current function's set of locally-assigned names. */
extern const std::unordered_set<std::string>* g_locals_assigned;

/***
 * @brief RAII guard that sets current locals-assigned set and restores on exit.
 */
struct ScopedLocalsAssigned {
  const std::unordered_set<std::string>* prev;
  explicit ScopedLocalsAssigned(const std::unordered_set<std::string>* cur);
  ~ScopedLocalsAssigned();
};

} // namespace pycc::sema::detail

