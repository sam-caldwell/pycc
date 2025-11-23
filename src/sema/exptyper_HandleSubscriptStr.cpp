/**
 * @file
 * @brief handleSubscriptStr: Type checks str indexing: index must be int; result is str.
 */
#include "sema/detail/exptyper/SubscriptHandlers.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/helpers/AddDiag.h"

namespace pycc::sema::detail {

static inline uint32_t maskOf(ast::TypeKind k, uint32_t set) { return set != 0U ? set : TypeEnv::maskForKind(k); }

bool handleSubscriptStr(const ast::Subscript& sub,
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
  out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); return true;
}

} // namespace pycc::sema::detail

