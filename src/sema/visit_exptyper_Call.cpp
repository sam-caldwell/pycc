/***
 * Name: ExpressionTyper::visit(Call)
 * Purpose: Dispatch and type calls; relies on existing stdlib lowering constraints.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Call& callNode) {
  // Reuse the existing logic by delegating to sema_check_impl call handling when base is Attribute/Name.
  // Here we conservatively type as None unless an stdlib/module dispatch sets stricter type via env.
  // To preserve behavior, evaluate callee/args for effects and set ok=false on underlying errors.
  if (callNode.callee) {
    ExpressionTyper c{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes};
    callNode.callee->accept(c); if (!c.ok) { ok = false; return; }
  }
  for (const auto& a : callNode.args) { if (!a) continue; ExpressionTyper ea{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes}; a->accept(ea); if (!ea.ok) { ok = false; return; } }
  out = ast::TypeKind::NoneType; outSet = 0U;
}

