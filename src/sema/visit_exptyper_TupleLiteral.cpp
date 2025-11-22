/***
 * Name: ExpressionTyper::visit(TupleLiteral)
 * Purpose: Type tuple literal by visiting children and setting canonical.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitContainers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::TupleLiteral& n) {
  auto visitChild = [&](const ast::Expr* e){ ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; e->accept(et); if (!et.ok) return false; return true; };
  if (!expr::handleTupleLiteral(n, out, outSet, visitChild)) { ok = false; }
}

