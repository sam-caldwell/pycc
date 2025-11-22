/***
 * Name: ExpressionTyper::visit(Name)
 * Purpose: Resolve name types from current and outer environments with locals enforcement.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/LocalsAssigned.h"
#include "sema/TypeEnv.h"
#include "ast/Name.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Name& n) {
  uint32_t maskVal = env->getSet(n.id);
  // Enforce strict local scoping: if this name is assigned locally anywhere in function,
  // treat it as a local and error if read before assignment.
  if (maskVal == 0U && pycc::sema::detail::g_locals_assigned && pycc::sema::detail::g_locals_assigned->count(n.id)) {
    addDiag(*diags, std::string("local variable referenced before assignment: ") + n.id, &n);
    ok = false; return;
  }
  if (maskVal == 0U && outers != nullptr) {
    for (const auto* o : *outers) { if (!o) continue; const uint32_t m = o->getSet(n.id); if (m != 0U) { maskVal = m; break; } }
  }
  if (maskVal == 0U) {
    // Try resolving from outer scopes only (free-variable reads)
    if (outers != nullptr) {
      uint32_t outerMask = 0U;
      for (const auto* o : *outers) { if (!o) continue; const uint32_t m = o->getSet(n.id); if (m != 0U) { outerMask = m; break; } }
      if (outerMask != 0U) { outSet = outerMask; if (TypeEnv::isSingleMask(outerMask)) out = TypeEnv::kindFromMask(outerMask); auto& nm = const_cast<ast::Name&>(n); nm.setType(out); nm.setCanonicalKey(std::string("n:") + n.id); return; }
      // Fall back to exact type in an outer scope when available
      std::optional<ast::TypeKind> oty;
      for (const auto* o : *outers) { if (!o) continue; auto t = o->get(n.id); if (t) { oty = t; break; } }
      if (oty) { out = *oty; outSet = TypeEnv::maskForKind(out); auto& nm = const_cast<ast::Name&>(n); nm.setType(out); nm.setCanonicalKey(std::string("n:") + n.id); return; }
    }
    // Contradictory or undefined at this scope
    addDiag(*diags, std::string("contradictory type for name: ") + n.id, &n);
    ok = false; return;
  }
  if (maskVal != 0U) {
    outSet = maskVal;
    if (TypeEnv::isSingleMask(maskVal)) { out = TypeEnv::kindFromMask(maskVal); }
  }
  auto resolvedType = env->get(n.id);
  if (!resolvedType && outSet == 0U) { addDiag(*diags, std::string("undefined name: ") + n.id, &n); ok = false; return; }
  if (TypeEnv::isSingleMask(outSet)) { out = TypeEnv::kindFromMask(outSet); }
  auto& mutableName = const_cast<ast::Name&>(n); // NOLINT(cppcoreguidelines-pro-type-const-cast)
  mutableName.setType(out); mutableName.setCanonicalKey(std::string("n:") + n.id);
}

