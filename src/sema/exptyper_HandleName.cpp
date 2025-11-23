/**
 * @file
 * @brief handleNameResolve: Resolve name types from current/outer environments.
 */
#include "sema/detail/exptyper/NameHandlers.h"
#include "sema/TypeEnv.h"
#include "sema/detail/helpers/AddDiag.h"

namespace pycc::sema::detail {

void handleNameResolve(const ast::Name& n,
                       const TypeEnv& env,
                       const std::vector<const TypeEnv*>* outers,
                       std::vector<Diagnostic>& diags,
                       ast::TypeKind& out,
                       uint32_t& outSet,
                       bool& ok) {
    uint32_t maskVal = env.getSet(n.id);
    if (maskVal == 0U && outers != nullptr) {
        for (const auto* o : *outers) { if (!o) continue; const uint32_t m = o->getSet(n.id); if (m != 0U) { maskVal = m; break; } }
    }
    if (maskVal == 0U) {
        if (outers != nullptr) {
            for (const auto* o : *outers) {
                if (!o) continue;
                auto t = o->get(n.id);
                if (t) { out = *t; outSet = TypeEnv::maskForKind(out); n.setType(out); n.setCanonicalKey(std::string("n:")+n.id); return; }
            }
        }
        addDiag(diags, std::string("contradictory type for name: ") + n.id, &n);
        ok = false;
        return;
    }
    outSet = maskVal;
    if (TypeEnv::isSingleMask(maskVal)) out = TypeEnv::kindFromMask(maskVal);
    n.setType(out);
    n.setCanonicalKey(std::string("n:")+n.id);
}

} // namespace pycc::sema::detail

