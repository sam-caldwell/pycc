/***
 * Name: pycc::sema::PolyRefs
 * Purpose: Mutable references to polymorphic call/attribute target maps.
 */
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace pycc::sema {

struct PolyRefs {
  std::unordered_map<std::string, std::unordered_set<std::string>>& vars;
  std::unordered_map<std::string, std::unordered_set<std::string>>& attrs;
};

} // namespace pycc::sema

