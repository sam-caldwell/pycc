/***
 * Name: ExpressionTyper::visit(Yield/Await/GeneratorExpr)
 * Purpose: Conservative typing for coroutine/generator constructs in subset.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "ast/YieldExpr.h"
#include "ast/AwaitExpr.h"
#include "ast/GeneratorExpr.h"
#include "ast/ListLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Name.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::YieldExpr& y) {
  (void)y; out = ast::TypeKind::NoneType; outSet = TypeEnv::maskForKind(out); ok = true;
}

void ExpressionTyper::visit(const ast::AwaitExpr& a) {
  (void)a; out = ast::TypeKind::NoneType; outSet = TypeEnv::maskForKind(out); ok = true;
}

void ExpressionTyper::visit(const ast::GeneratorExpr& ge) {
  TypeEnv local = *env;
  auto inferElemMask = [&](const ast::Expr* it) -> uint32_t { if (!it) return 0U; if (it->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(it); const uint32_t e = local.getListElems(nm->id); if (e != 0U) return e; } if (it->kind == ast::NodeKind::ListLiteral) { uint32_t em = 0U; const auto* lst = static_cast<const ast::ListLiteral*>(it); for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; el->accept(et); if (!et.ok) return 0U; em |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); } return em; } return 0U; };
  const ast::Expr* currentIter = nullptr;
  std::function<void(const ast::Expr*, uint32_t)> bindTarget = [&](const ast::Expr* tgt, uint32_t elemMask) {
    if (!tgt) return;
    if (tgt->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(tgt); uint32_t m = elemMask; if (m == 0U) m = TypeEnv::maskForKind(ast::TypeKind::Int); local.defineSet(nm->id, m, {"<comp>", 0, 0}); }
    else if (tgt->kind == ast::NodeKind::TupleLiteral) {
      const auto* tp = static_cast<const ast::TupleLiteral*>(tgt);
      for (size_t i = 0; i < tp->elements.size(); ++i) { const auto& e = tp->elements[i]; if (!e) continue; bindTarget(e.get(), elemMask); }
    }
  };
  for (const auto& f : ge.fors) {
    if (f.iter) { ExpressionTyper it{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; f.iter->accept(it); if (!it.ok) { ok = false; return; } }
    currentIter = f.iter.get();
    uint32_t em = inferElemMask(f.iter.get()); bindTarget(f.target.get(), em);
    for (const auto& g : f.ifs) {
      if (!g) continue;
      ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
      g->accept(et); if (!et.ok) { ok = false; return; }
      if (!typeIsBool(et.out)) {
        // Relaxation: allow name-based truthiness over numeric types in generator guards
        if (g->kind == ast::NodeKind::Name) {
          const auto* nm = static_cast<const ast::Name*>(g.get());
          const uint32_t m = local.getSet(nm->id);
          const uint32_t numMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
          if (m != 0U && (m & ~numMask) == 0U) {
            continue; // accept numeric name as truthy
          }
        }
        addDiag(*diags, "generator guard must be bool", g.get()); ok = false; return;
      }
    }
  }
  if (ge.elt) { ExpressionTyper et{local, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; ge.elt->accept(et); if (!et.ok) { ok = false; return; } }
  // Treat generator expr as List for typing in this subset
  out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out);
}

