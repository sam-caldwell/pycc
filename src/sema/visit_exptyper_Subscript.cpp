/***
 * Name: ExpressionTyper::visit(Subscript)
 * Purpose: Type subscript over str/list/tuple/dict with constraints and diagnostics.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Subscript& sub) {
  if (!sub.value) { addDiag(*diags, "null subscript", &sub); ok = false; return; }
  if (sub.value->kind == ast::NodeKind::SetLiteral) { addDiag(*diags, "set is not subscriptable", &sub); ok = false; return; }
  ExpressionTyper v{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers};
  sub.value->accept(v); if (!v.ok) { ok = false; return; }
  const uint32_t vMask = (v.outSet != 0U) ? v.outSet : TypeEnv::maskForKind(v.out);
  auto isSubset = [](uint32_t m, uint32_t allow){ return m && ((m & ~allow)==0U); };
  const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int);
  const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
  const uint32_t listMask = TypeEnv::maskForKind(ast::TypeKind::List);
  const uint32_t tupMask = TypeEnv::maskForKind(ast::TypeKind::Tuple);
  const uint32_t dictMask = TypeEnv::maskForKind(ast::TypeKind::Dict);
  if (vMask == strMask) {
    if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
    out = ast::TypeKind::Str; outSet = strMask; auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
  }
  if (vMask == listMask) {
    if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
    uint32_t elemMask = 0U;
    if (sub.value->kind == ast::NodeKind::Name) {
      const auto* nm = static_cast<const ast::Name*>(sub.value.get());
      elemMask = env->getListElems(nm->id);
    } else if (sub.value->kind == ast::NodeKind::ListLiteral) {
      const auto* lst = static_cast<const ast::ListLiteral*>(sub.value.get());
      for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
    }
    if (elemMask != 0U) { outSet = elemMask; if (TypeEnv::isSingleMask(elemMask)) out = TypeEnv::kindFromMask(elemMask); }
    else { out = ast::TypeKind::NoneType; outSet = 0U; }
    auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
  }
  if (vMask == tupMask) {
    if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
    out = ast::TypeKind::NoneType; outSet = 0U; auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
  }
  if (vMask == dictMask) {
    uint32_t keyMask = 0U, valMask = 0U;
    if (sub.value->kind == ast::NodeKind::Name) {
      const auto* nm = static_cast<const ast::Name*>(sub.value.get());
      keyMask = env->getDictKeys(nm->id);
      valMask = env->getDictVals(nm->id);
    } else if (sub.value->kind == ast::NodeKind::DictLiteral) {
      const auto* dl = static_cast<const ast::DictLiteral*>(sub.value.get());
      for (const auto& kv : dl->items) {
        if (kv.first) { ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; kv.first->accept(kt); if (!kt.ok) { ok = false; return; } keyMask |= (kt.outSet != 0U) ? kt.outSet : TypeEnv::maskForKind(kt.out); }
        if (kv.second) { ExpressionTyper vt{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; kv.second->accept(vt); if (!vt.ok) { ok = false; return; } valMask |= (vt.outSet != 0U) ? vt.outSet : TypeEnv::maskForKind(vt.out); }
      }
    }
    if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (keyMask != 0U && !isSubset(sMask, keyMask)) { addDiag(*diags, "dict key type mismatch", &sub); ok = false; return; } }
    if (valMask != 0U) { outSet = valMask; if (TypeEnv::isSingleMask(valMask)) out = TypeEnv::kindFromMask(valMask); }
    else { out = ast::TypeKind::NoneType; outSet = 0U; }
    auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
  }
  addDiag(*diags, "unsupported subscript target type", &sub); ok = false; return;
}

