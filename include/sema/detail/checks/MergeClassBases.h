/**
 * @file
 * @brief Merge inherited methods from base classes into each ClassInfo.
 */
#pragma once

#include <unordered_map>
#include <string>
#include "sema/detail/Types.h"

namespace pycc::sema::detail {

/** Merge methods from each class's base list recursively. */
void mergeClassBases(std::unordered_map<std::string, ClassInfo>& classes);

} // namespace pycc::sema::detail

