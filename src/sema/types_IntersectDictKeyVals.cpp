/**
 * @file
 * @brief IntersectOps::dictKeyVals: Intersect dict key/value masks per name across envs.
 */
#include "sema/TypeEnv.h"
#include "sema/detail/types/IntersectOps.h"

namespace pycc::sema::detail {

void IntersectOps::dictKeyVals(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b) {
    for (const auto& kv : a.dictKeySets_) {
        const std::string& name = kv.first; auto itb = b.dictKeySets_.find(name);
        if (itb != b.dictKeySets_.end()) dst.dictKeySets_[name] = (kv.second & itb->second);
    }
    for (const auto& kv : a.dictValSets_) {
        const std::string& name = kv.first; auto itb = b.dictValSets_.find(name);
        if (itb != b.dictValSets_.end()) dst.dictValSets_[name] = (kv.second & itb->second);
    }
}

} // namespace pycc::sema::detail

