/***
 * Name: ExpressionTyper::visit(ListComp)
 * Purpose: Type-check list comprehensions with local env and bool guards.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "ast/Nodes.h"
#include <functional>
#include <cstdint>
#include "ast/ListLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Name.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::ListComp &lc) {
    TypeEnv local = *env; // isolate comp scope
    auto typeMask = [&](ast::TypeKind k, uint32_t prev) { return prev != 0U ? prev : TypeEnv::maskForKind(k); };

    auto listElemMask = [&](const ast::ListLiteral* lst) -> uint32_t {
        uint32_t em = 0U;
        for (const auto& el : lst->elements) {
            if (!el) continue;
            ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
            el->accept(et); if (!et.ok) return 0U;
            em |= typeMask(et.out, et.outSet);
        }
        return em;
    };
    auto inferElemMask = [&](const ast::Expr* it) -> uint32_t {
        if (!it) return 0U;
        if (it->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(it); const uint32_t e = local.getListElems(nm->id); if (e != 0U) return e; }
        if (it->kind == ast::NodeKind::ListLiteral) return listElemMask(static_cast<const ast::ListLiteral*>(it));
        return 0U;
    };
    const ast::Expr* currentIter = nullptr;
    auto computePerIndex = [&](const ast::TupleLiteral* tp, const ast::Expr* iter, int parentIdx) -> std::vector<uint32_t> {
        std::vector<uint32_t> perIndex; if (!iter || iter->kind != ast::NodeKind::ListLiteral) return perIndex;
        const auto* lst = static_cast<const ast::ListLiteral*>(iter);
        size_t arity = tp->elements.size(); perIndex.assign(arity, 0U);
        for (const auto& el : lst->elements) {
            if (!el || el->kind != ast::NodeKind::TupleLiteral) continue;
            const auto* lt = static_cast<const ast::TupleLiteral*>(el.get());
            const ast::TupleLiteral* inner = lt;
            if (parentIdx >= 0 && parentIdx < static_cast<int>(lt->elements.size()) && lt->elements[parentIdx] && lt->elements[parentIdx]->kind == ast::NodeKind::TupleLiteral) {
                inner = static_cast<const ast::TupleLiteral*>(lt->elements[parentIdx].get());
            }
            for (size_t i = 0; i < std::min(arity, inner->elements.size()); ++i) {
                const auto& sub = inner->elements[i]; if (!sub) continue;
                ExpressionTyper set{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
                sub->accept(set); if (!set.ok) { ok = false; return std::vector<uint32_t>{}; }
                perIndex[i] |= typeMask(set.out, set.outSet);
            }
        }
        return perIndex;
    };
    std::function<void(const ast::Expr*, uint32_t, int)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask, int parentIdx) {
        if (!tgt) return;
        if (tgt->kind == ast::NodeKind::Name) {
            const auto* nm = static_cast<const ast::Name*>(tgt);
            uint32_t m = elemMask == 0U ? TypeEnv::maskForKind(ast::TypeKind::Int) : elemMask;
            local.defineSet(nm->id, m, {"<comp>", 0, 0});
            return;
        }
        if (tgt->kind == ast::NodeKind::TupleLiteral) {
            const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
            const ast::Name* iterName = (currentIter && currentIter->kind == ast::NodeKind::Name) ? static_cast<const ast::Name*>(currentIter) : nullptr;
            std::vector<uint32_t> perIndex = computePerIndex(tp, currentIter, parentIdx);
            for (size_t i = 0; i < tp->elements.size(); ++i) {
                const auto& e = tp->elements[i]; if (!e) continue;
                uint32_t m = elemMask;
                if (iterName) { const uint32_t mi = local.getTupleElemAt(iterName->id, i); if (mi != 0U) m = mi; }
                else if (!perIndex.empty() && i < perIndex.size() && perIndex[i] != 0U) { m = perIndex[i]; }
                const int nextParentIdx = (parentIdx >= 0) ? parentIdx : static_cast<int>(i);
                bindTarget(e.get(), m, nextParentIdx);
            }
        }
    };

    for (const auto& f : lc.fors) {
        if (f.iter) { ExpressionTyper it{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; f.iter->accept(it); if (!it.ok) { ok = false; return; } }
        currentIter = f.iter.get();
        const uint32_t em = inferElemMask(f.iter.get());
        bindTarget(f.target.get(), em, -1);
        for (const auto& g : f.ifs) {
            if (!g) continue;
            ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; g->accept(et); if (!et.ok) { ok = false; return; }
            if (!typeIsBool(et.out)) { addDiag(*diags, "list comprehension guard must be bool", g.get()); ok = false; return; }
        }
    }
    if (lc.elt) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; lc.elt->accept(et); if (!et.ok) { ok = false; return; } }
    out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out); lc.setType(out);
}
