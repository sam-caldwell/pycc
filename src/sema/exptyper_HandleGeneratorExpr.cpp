/**
 * @file
 * @brief handleGeneratorExpr: Conservative typing for generator expressions.
 */
#include "sema/detail/exptyper/CompHandlers.h"
#include "sema/detail/ExpressionTyper.h"
#include <functional>

namespace pycc::sema::detail {

bool handleGeneratorExpr(const ast::GeneratorExpr& ge,
                         const TypeEnv& env,
                         const std::unordered_map<std::string, Sig>& sigs,
                         const std::unordered_map<std::string, int>& retParamIdxs,
                         std::vector<Diagnostic>& diags,
                         PolyPtrs polyTargets,
                         const std::vector<const TypeEnv*>* outers,
                         const std::unordered_map<std::string, ClassInfo>* classes,
                         ast::TypeKind& out,
                         uint32_t& outSet,
                         bool& ok) {
    TypeEnv local = env;
    auto inferElemMask = [&](const ast::Expr* it) -> uint32_t {
        if (!it) return 0U;
        if (it->kind == ast::NodeKind::Name) {
            const auto* nm = static_cast<const ast::Name*>(it);
            const uint32_t e = local.getListElems(nm->id);
            if (e != 0U) return e;
        }
        if (it->kind == ast::NodeKind::ListLiteral) {
            uint32_t em = 0U;
            const auto* lst = static_cast<const ast::ListLiteral*>(it);
            for (const auto& el : lst->elements) {
                if (!el) continue;
                ExpressionTyper et{local, sigs, retParamIdxs, diags, polyTargets, outers, classes};
                el->accept(et);
                if (!et.ok) return 0U;
                em |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out);
            }
            return em;
        }
        return 0U;
    };
    std::function<void(const ast::Expr*, uint32_t)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask) {
        if (!tgt) return;
        if (tgt->kind == ast::NodeKind::Name) {
            const auto* nm = static_cast<const ast::Name*>(tgt);
            uint32_t m = elemMask == 0U ? TypeEnv::maskForKind(ast::TypeKind::Int) : elemMask;
            local.defineSet(nm->id, m, {"<comp>", 0, 0});
        } else if (tgt->kind == ast::NodeKind::TupleLiteral) {
            const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
            for (const auto& e : tp->elements) { if (!e) continue; bindTarget(e.get(), elemMask); }
        }
    };
    for (const auto& f : ge.fors) {
        if (f.iter) {
            ExpressionTyper it{local, sigs, retParamIdxs, diags, polyTargets, outers, classes};
            f.iter->accept(it);
            if (!it.ok) {
                ok = false;
                return true;
            }
        }
        uint32_t em = inferElemMask(f.iter.get());
        bindTarget(f.target.get(), em);
        for (const auto& g : f.ifs) {
            if (!g) continue;
            ExpressionTyper et{local, sigs, retParamIdxs, diags, polyTargets, outers, classes};
            g->accept(et);
            if (!et.ok) { ok = false; return true; }
            if (!typeIsBool(et.out)) {
                // Relaxation: allow name-based truthiness over numeric types in generator guards
                if (g->kind == ast::NodeKind::Name) {
                    const auto* nm = static_cast<const ast::Name*>(g.get());
                    const uint32_t m = local.getSet(nm->id);
                    const uint32_t numMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
                    if (m != 0U && (m & ~numMask) == 0U) { continue; }
                }
                addDiag(diags, "generator guard must be bool", g.get()); ok = false; return true;
            }
        }
    }
    if (ge.elt) {
        ExpressionTyper et{local, sigs, retParamIdxs, diags, polyTargets, outers, classes};
        ge.elt->accept(et);
        if (!et.ok) {
            ok = false;
            return true;
        }
    }
    out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out);
    return true;
}

} // namespace pycc::sema::detail
