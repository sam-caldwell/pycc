/***
 * Name: ScopedLocalsAssigned
 * Purpose: RAII guard that sets current locals-assigned set and restores on exit.
 */
#pragma once

#include <string>
#include <unordered_set>

namespace pycc::sema::detail {

struct ScopedLocalsAssigned {
  const std::unordered_set<std::string>* prev;
  explicit ScopedLocalsAssigned(const std::unordered_set<std::string>* cur);
  ~ScopedLocalsAssigned();
};

} // namespace pycc::sema::detail

