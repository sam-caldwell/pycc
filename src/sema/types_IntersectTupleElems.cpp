/**
 * @file
 * @brief IntersectOps::tupleElems: Intersect tuple element masks index-wise for names.
 */
#include "sema/TypeEnv.h"
#include "sema/detail/types/IntersectOps.h"

namespace pycc::sema::detail {

void IntersectOps::tupleElems(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b) {
    for (const auto& kv : a.tupleElemSets_) {
        const std::string& name = kv.first; auto itb = b.tupleElemSets_.find(name);
        if (itb == b.tupleElemSets_.end()) continue;
        const auto& va = kv.second; const auto& vb = itb->second; const size_t n = std::min(va.size(), vb.size());
        std::vector<uint32_t> out; out.reserve(n);
        for (size_t i = 0; i < n; ++i) out.push_back(va[i] & vb[i]);
        dst.tupleElemSets_[name] = std::move(out);
    }
}

} // namespace pycc::sema::detail

