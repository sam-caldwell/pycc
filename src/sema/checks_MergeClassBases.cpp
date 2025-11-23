/**
 * @file
 * @brief mergeClassBases: Recursively merge base class methods.
 */
#include "sema/detail/checks/MergeClassBases.h"

namespace pycc::sema::detail {

static void mergeFrom(const std::unordered_map<std::string, ClassInfo>& all,
                      const std::string& base,
                      ClassInfo& into) {
  auto it = all.find(base); if (it == all.end()) return;
  for (const auto& mkv : it->second.methods) {
    if (!into.methods.contains(mkv.first)) into.methods[mkv.first] = mkv.second;
  }
  for (const auto& b : it->second.bases) mergeFrom(all, b, into);
}

void mergeClassBases(std::unordered_map<std::string, ClassInfo>& classes) {
  for (auto& kv : classes) {
    for (const auto& b : kv.second.bases) mergeFrom(classes, b, kv.second);
  }
}

} // namespace pycc::sema::detail

