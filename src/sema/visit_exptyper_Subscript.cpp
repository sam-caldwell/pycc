/***
 * Name: ExpressionTyper::visit(Subscript)
 * Purpose: Type-check value[index] for str/list/tuple/dict.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Helpers.h"
#include "ast/Subscript.h"
#include "ast/Name.h"
#include "ast/ListLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/DictLiteral.h"
#include "ast/IntLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Subscript& sub) {
  // value[index] typing for list/str/tuple/dict; sets are not subscriptable
  if (!sub.value) { addDiag(*diags, "null subscript", &sub); ok = false; return; }
  // Reject set literals directly
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
  // String indexing -> str; index must be int
  if (vMask == strMask) {
    if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
    out = ast::TypeKind::Str; outSet = strMask; auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
  }
  // List indexing -> element mask; index must be int
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
  // Tuple indexing -> element type at index, or union when unknown; index must be int
  if (vMask == tupMask || sub.value->kind == ast::NodeKind::TupleLiteral) {
    if (sub.slice) { ExpressionTyper s{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return; } const uint32_t sMask = (s.outSet != 0U) ? s.outSet : TypeEnv::maskForKind(s.out); if (!isSubset(sMask, iMask)) { addDiag(*diags, "subscript index must be int", &sub); ok = false; return; } }
    uint32_t elemMask = 0U;
    size_t idxVal = static_cast<size_t>(-1);
    bool haveConstIdx = false;
    if (sub.slice && sub.slice->kind == ast::NodeKind::IntLiteral) { const auto* lit = static_cast<const ast::IntLiteral*>(sub.slice.get()); if (lit->value >= 0) { idxVal = static_cast<size_t>(lit->value); haveConstIdx = true; } }
    if (sub.value->kind == ast::NodeKind::Name) {
      const auto* nm = static_cast<const ast::Name*>(sub.value.get());
      if (haveConstIdx) { elemMask = env->getTupleElemAt(nm->id, idxVal); }
      if (elemMask == 0U) { elemMask = env->unionOfTupleElems(nm->id); }
    } else if (sub.value->kind == ast::NodeKind::TupleLiteral) {
      const auto* tup = static_cast<const ast::TupleLiteral*>(sub.value.get());
      if (haveConstIdx && idxVal < tup->elements.size()) {
        if (tup->elements[idxVal]) { ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; tup->elements[idxVal]->accept(et); if (!et.ok) { ok = false; return; } elemMask = (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
      } else {
        for (const auto& el : tup->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
      }
    }
    if (elemMask != 0U) { outSet = elemMask; if (TypeEnv::isSingleMask(elemMask)) out = TypeEnv::kindFromMask(elemMask); }
    else { out = ast::TypeKind::NoneType; outSet = 0U; }
    auto& m = const_cast<ast::Subscript&>(sub); m.setType(out); return;
  }
  // Dict indexing -> value mask; key type must match known key set when available
  if (vMask == dictMask || sub.value->kind == ast::NodeKind::DictLiteral) {
    uint32_t keyMask = 0U;
    uint32_t valMask = 0U;
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

