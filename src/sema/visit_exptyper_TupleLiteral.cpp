/***
 * Name: ExpressionTyper::visit(TupleLiteral)
 * Purpose: Delegate to container helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitContainers.h"
#include "ast/TupleLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::TupleLiteral& tupleLiteral) {
  auto visitChild = [&](const ast::Expr* e){
    ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    e->accept(et); if (!et.ok) return false; return true;
  };
  if (!expr::handleTupleLiteral(tupleLiteral, out, outSet, visitChild)) { ok = false; return; }
}

