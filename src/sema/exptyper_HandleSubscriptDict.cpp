/**
 * @file
 * @brief handleSubscriptDict: Type checks dict indexing; key type match; returns value mask.
 */
#include "sema/detail/exptyper/SubscriptHandlers.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/helpers/AddDiag.h"
#include "ast/Name.h"
#include "ast/DictLiteral.h"

namespace pycc::sema::detail {

static inline uint32_t maskOf(ast::TypeKind k, uint32_t set) { return set != 0U ? set : TypeEnv::maskForKind(k); }

bool handleSubscriptDict(const ast::Subscript& sub,
                         const TypeEnv& env,
                         const std::unordered_map<std::string, Sig>& sigs,
                         const std::unordered_map<std::string, int>& retParamIdxs,
                         std::vector<Diagnostic>& diags,
                         PolyPtrs poly,
                         const std::vector<const TypeEnv*>* outers,
                         ast::TypeKind& out,
                         uint32_t& outSet,
                         bool& ok) {
  uint32_t keyMask = 0U, valMask = 0U;
  if (sub.value && sub.value->kind == ast::NodeKind::Name) {
    const auto* nm = static_cast<const ast::Name*>(sub.value.get()); keyMask = env.getDictKeys(nm->id); valMask = env.getDictVals(nm->id);
  } else if (sub.value && sub.value->kind == ast::NodeKind::DictLiteral) {
    const auto* dl = static_cast<const ast::DictLiteral*>(sub.value.get());
    for (const auto& kv : dl->items) {
      if (kv.first) { ExpressionTyper kt{env, sigs, retParamIdxs, diags, poly, outers}; kv.first->accept(kt); if (!kt.ok) { ok = false; return true; } keyMask |= maskOf(kt.out, kt.outSet); }
      if (kv.second) { ExpressionTyper vt{env, sigs, retParamIdxs, diags, poly, outers}; kv.second->accept(vt); if (!vt.ok) { ok = false; return true; } valMask |= maskOf(vt.out, vt.outSet); }
    }
  }
  if (sub.slice) {
    ExpressionTyper s{env, sigs, retParamIdxs, diags, poly, outers}; sub.slice->accept(s); if (!s.ok) { ok = false; return true; }
    const uint32_t sMask = maskOf(s.out, s.outSet);
    if (keyMask != 0U && !(sMask && ((sMask & ~keyMask) == 0U))) { addDiag(diags, "dict key type mismatch", &sub); ok = false; return true; }
  }
  if (valMask != 0U) { outSet = valMask; if (TypeEnv::isSingleMask(valMask)) out = TypeEnv::kindFromMask(valMask); }
  else { out = ast::TypeKind::NoneType; outSet = 0U; }
  return true;
}

} // namespace pycc::sema::detail

