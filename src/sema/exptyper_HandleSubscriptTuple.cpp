/**
 * @file
 * @brief handleSubscriptTuple: Type checks tuple indexing; index must be int; element mask or union.
 */
#include "sema/detail/exptyper/SubscriptHandlers.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/helpers/AddDiag.h"
#include "ast/TupleLiteral.h"
#include "ast/IntLiteral.h"
#include "ast/Name.h"

namespace pycc::sema::detail {

static inline uint32_t maskOf(ast::TypeKind k, uint32_t set) { return set != 0U ? set : TypeEnv::maskForKind(k); }

bool handleSubscriptTuple(const ast::Subscript& sub,
                          const TypeEnv& env,
                          const std::unordered_map<std::string, Sig>& sigs,
                          const std::unordered_map<std::string, int>& retParamIdxs,
                          std::vector<Diagnostic>& diags,
                          PolyPtrs poly,
                          const std::vector<const TypeEnv*>* outers,
                          ast::TypeKind& out,
                          uint32_t& outSet,
                          bool& ok) {
  const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int);
  if (sub.slice) {
    ExpressionTyper s{env, sigs, retParamIdxs, diags, poly, outers}; sub.slice->accept(s);
    if (!s.ok) { ok = false; return true; }
    const uint32_t sMask = maskOf(s.out, s.outSet);
    if (!(sMask && ((sMask & ~iMask) == 0U))) { addDiag(diags, "subscript index must be int", &sub); ok = false; return true; }
  }
  uint32_t elemMask = 0U; size_t idxVal = static_cast<size_t>(-1); bool haveConstIdx = false;
  if (sub.slice && sub.slice->kind == ast::NodeKind::IntLiteral) {
    const auto* lit = static_cast<const ast::IntLiteral*>(sub.slice.get()); if (lit->value >= 0) { idxVal = static_cast<size_t>(lit->value); haveConstIdx = true; }
  }
  if (sub.value && sub.value->kind == ast::NodeKind::Name) {
    const auto* nm = static_cast<const ast::Name*>(sub.value.get()); if (haveConstIdx) { elemMask = env.getTupleElemAt(nm->id, idxVal); } if (elemMask == 0U) elemMask = env.unionOfTupleElems(nm->id);
  } else if (sub.value && sub.value->kind == ast::NodeKind::TupleLiteral) {
    const auto* tup = static_cast<const ast::TupleLiteral*>(sub.value.get());
    if (haveConstIdx && idxVal < tup->elements.size() && tup->elements[idxVal]) {
      ExpressionTyper et{env, sigs, retParamIdxs, diags, poly, outers}; tup->elements[idxVal]->accept(et); if (!et.ok) { ok = false; return true; }
      elemMask = maskOf(et.out, et.outSet);
    } else {
      for (const auto& el : tup->elements) { if (!el) continue; ExpressionTyper et{env, sigs, retParamIdxs, diags, poly, outers}; el->accept(et); if (!et.ok) { ok = false; return true; } elemMask |= maskOf(et.out, et.outSet); }
    }
  }
  if (elemMask != 0U) { outSet = elemMask; if (TypeEnv::isSingleMask(elemMask)) out = TypeEnv::kindFromMask(elemMask); }
  else { out = ast::TypeKind::NoneType; outSet = 0U; }
  return true;
}

} // namespace pycc::sema::detail

