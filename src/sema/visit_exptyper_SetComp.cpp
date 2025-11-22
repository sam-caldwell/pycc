/***
 * Name: ExpressionTyper::visit(SetComp)
 * Purpose: Treat set comps as List for typing; check guards.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "ast/SetComp.h"
#include "ast/ListLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Name.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::SetComp& sc) {
  TypeEnv local = *env;
  auto inferElemMask = [&](const ast::Expr* it) -> uint32_t {
    if (!it) return 0U;
    if (it->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(it); const uint32_t e = local.getListElems(nm->id); if (e != 0U) return e; }
    if (it->kind == ast::NodeKind::ListLiteral) {
      uint32_t em = 0U; const auto* lst = static_cast<const ast::ListLiteral*>(it);
      for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; el->accept(et); if (!et.ok) return 0U; em |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
      return em;
    }
    return 0U;
  };
  const ast::Expr* currentIter = nullptr;
  std::function<void(const ast::Expr*, uint32_t)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask) {
    if (!tgt) return;
    if (tgt->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(tgt); uint32_t m = elemMask; if (m == 0U) m = TypeEnv::maskForKind(ast::TypeKind::Int); local.defineSet(nm->id, m, {"<comp>", 0, 0}); }
    else if (tgt->kind == ast::NodeKind::TupleLiteral) {
      const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
      for (size_t i = 0; i < tp->elements.size(); ++i) { const auto& e = tp->elements[i]; if (!e) continue; bindTarget(e.get(), elemMask); }
    }
  };
  for (const auto& f : sc.fors) {
    if (f.iter) { ExpressionTyper it{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; f.iter->accept(it); if (!it.ok) { ok = false; return; } }
    currentIter = f.iter.get(); (void)currentIter;
    uint32_t em = inferElemMask(f.iter.get()); bindTarget(f.target.get(), em);
    for (const auto& g : f.ifs) { if (!g) continue; ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; g->accept(et); if (!et.ok) { ok = false; return; } if (!typeIsBool(et.out)) { addDiag(*diags, "set comprehension guard must be bool", g.get()); ok = false; return; } }
  }
  if (sc.elt) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; sc.elt->accept(et); if (!et.ok) { ok = false; return; } }
  out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out);
}

