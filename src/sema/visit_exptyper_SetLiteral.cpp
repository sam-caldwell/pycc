/***
 * Name: ExpressionTyper::visit(SetLiteral)
 * Purpose: Validate children; type conservatively as List in subset.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "ast/SetLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::SetLiteral& setLiteral) {
  for (const auto& element : setLiteral.elements) {
    if (!element) continue;
    ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    element->accept(et); if (!et.ok) { ok = false; return; }
  }
  out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out);
}

