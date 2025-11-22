/***
 * Name: ExpressionTyper::visit(ObjectLiteral)
 * Purpose: Type object literal by visiting fields and setting canonical.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitContainers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::ObjectLiteral& n) {
  auto visitChild = [&](const ast::Expr* e){ ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; e->accept(et); if (!et.ok) return false; return true; };
  if (!expr::handleObjectLiteral(n, out, outSet, visitChild)) { ok = false; }
}

