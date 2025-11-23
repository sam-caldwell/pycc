/**
 * @file
 * @brief handleSubscriptList: Type checks list indexing; index must be int; element mask.
 */
#include "sema/detail/exptyper/SubscriptHandlers.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/helpers/AddDiag.h"
#include "ast/Name.h"
#include "ast/ListLiteral.h"

namespace pycc::sema::detail {

static inline uint32_t maskOf(ast::TypeKind k, uint32_t set) { return set != 0U ? set : TypeEnv::maskForKind(k); }

bool handleSubscriptList(const ast::Subscript& sub,
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
    if (const uint32_t sMask = maskOf(s.out, s.outSet); !(sMask && ((sMask & ~iMask) == 0U))) { addDiag(diags, "subscript index must be int", &sub); ok = false; return true; }
  }
  uint32_t elemMask = 0U;
  if (sub.value && sub.value->kind == ast::NodeKind::Name) {
    const auto* nm = static_cast<const ast::Name*>(sub.value.get()); elemMask = env.getListElems(nm->id);
  } else if (sub.value && sub.value->kind == ast::NodeKind::ListLiteral) {
    const auto* lst = static_cast<const ast::ListLiteral*>(sub.value.get());
    for (const auto& el : lst->elements) {
      if (!el) continue; ExpressionTyper et{env, sigs, retParamIdxs, diags, poly, outers}; el->accept(et); if (!et.ok) { ok = false; return true; }
      elemMask |= maskOf(et.out, et.outSet);
    }
  }
  if (elemMask != 0U) { outSet = elemMask; if (TypeEnv::isSingleMask(elemMask)) out = TypeEnv::kindFromMask(elemMask); }
  else { out = ast::TypeKind::NoneType; outSet = 0U; }
  return true;
}

} // namespace pycc::sema::detail

