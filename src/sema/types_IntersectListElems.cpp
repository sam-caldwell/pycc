/**
 * @file
 * @brief IntersectOps::listElems: Intersect per-name list element masks across envs.
 */
#include "sema/TypeEnv.h"
#include "sema/detail/types/IntersectOps.h"

namespace pycc::sema::detail {

void IntersectOps::listElems(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b) {
    for (const auto& kv : a.listElemSets_) {
        const std::string& name = kv.first; auto itb = b.listElemSets_.find(name);
        if (itb == b.listElemSets_.end()) continue; dst.listElemSets_[name] = (kv.second & itb->second);
    }
}

} // namespace pycc::sema::detail

