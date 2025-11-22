/***
 * Name: ExpressionTyper::visit(Name)
 * Purpose: Resolve name types using current and outer envs; enforce unbound local error.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/LocalsAssigned.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Name& n) {
  uint32_t maskVal = env->getSet(n.id);
  if (maskVal == 0U && detail::g_locals_assigned && detail::g_locals_assigned->count(n.id)) {
    addDiag(*diags, std::string("local variable referenced before assignment: ") + n.id, &n);
    ok = false; return;
  }
  if (maskVal == 0U && outers != nullptr) {
    for (const auto* o : *outers) { if (!o) continue; const uint32_t m = o->getSet(n.id); if (m != 0U) { maskVal = m; break; } }
  }
  if (maskVal == 0U) {
    if (outers != nullptr) {
      uint32_t outerMask = 0U;
      for (const auto* o : *outers) { if (!o) continue; const uint32_t m = o->getSet(n.id); if (m != 0U) { outerMask = m; break; } }
      if (outerMask != 0U) { outSet = outerMask; if (TypeEnv::isSingleMask(outerMask)) out = TypeEnv::kindFromMask(outerMask); auto& nm = const_cast<ast::Name&>(n); nm.setType(out); nm.setCanonicalKey(std::string("n:") + n.id); return; }
      std::optional<ast::TypeKind> oty;
      for (const auto* o : *outers) { if (!o) continue; auto t = o->get(n.id); if (t) { oty = t; break; } }
      if (oty) { out = *oty; outSet = TypeEnv::maskForKind(out); auto& nm = const_cast<ast::Name&>(n); nm.setType(out); nm.setCanonicalKey(std::string("n:") + n.id); return; }
    }
    addDiag(*diags, std::string("contradictory type for name: ") + n.id, &n); ok = false; return;
  }
  if (maskVal != 0U) { outSet = maskVal; if (TypeEnv::isSingleMask(maskVal)) { out = TypeEnv::kindFromMask(maskVal); } }
  auto resolvedType = env->get(n.id);
  if (!resolvedType && outSet == 0U) { addDiag(*diags, std::string("undefined name: ") + n.id, &n); ok = false; return; }
  if (TypeEnv::isSingleMask(outSet)) { out = TypeEnv::kindFromMask(outSet); }
  auto& mutableName = const_cast<ast::Name&>(n);
  mutableName.setType(out); mutableName.setCanonicalKey(std::string("n:") + n.id);
}

