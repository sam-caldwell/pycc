/***
 * Name: pycc::sema::PolyPtrs
 * Purpose: Immutable pointers to polymorphic call/attribute target maps.
 */
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace pycc::sema {

struct PolyPtrs {
  const std::unordered_map<std::string, std::unordered_set<std::string>>* vars{nullptr};
  const std::unordered_map<std::string, std::unordered_set<std::string>>* attrs{nullptr};
};

} // namespace pycc::sema

