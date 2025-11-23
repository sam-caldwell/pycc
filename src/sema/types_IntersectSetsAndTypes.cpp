/**
 * @file
 * @brief IntersectOps::setsAndTypes: Intersect scalar masks and propagate singular kinds.
 */
#include "sema/TypeEnv.h"
#include "sema/detail/types/IntersectOps.h"

namespace pycc::sema::detail {

void IntersectOps::setsAndTypes(TypeEnv& dst, const TypeEnv& a, const TypeEnv& b) {
    for (const auto& kv : a.sets_) {
        const std::string& name = kv.first; const uint32_t am = kv.second;
        auto itb = b.sets_.find(name); if (itb == b.sets_.end()) continue; const uint32_t bm = itb->second;
        if (am == 0U || bm == 0U) continue; const uint32_t inter = am & bm;
        dst.sets_[name] = inter;
        if (inter != 0U && dst.isSingle(inter)) { dst.types_[name] = dst.kindFor(inter); }
    }
    for (const auto& kv : b.sets_) {
        const std::string& name = kv.first; if (a.sets_.contains(name)) continue;
        const uint32_t bm = kv.second; const uint32_t am = a.getSet(name);
        if (am == 0U || bm == 0U) continue; const uint32_t inter = am & bm;
        dst.sets_[name] = inter;
        if (inter != 0U && dst.isSingle(inter)) { dst.types_[name] = dst.kindFor(inter); }
    }
}

} // namespace pycc::sema::detail

