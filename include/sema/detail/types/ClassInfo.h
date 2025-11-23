/***
 * Name: pycc::sema::ClassInfo
 * Purpose: Minimal per-class info for base list and method signatures.
 */
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "sema/detail/types/Sig.h"

namespace pycc::sema {

struct ClassInfo {
  std::vector<std::string> bases;
  std::unordered_map<std::string, Sig> methods; // method name -> signature
};

} // namespace pycc::sema

