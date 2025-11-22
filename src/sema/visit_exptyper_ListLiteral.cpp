/***
 * Name: ExpressionTyper::visit(ListLiteral)
 * Purpose: Delegate to container helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitContainers.h"
#include "ast/ListLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::ListLiteral& listLiteral) {
  auto visitChild = [&](const ast::Expr* e){
    ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    e->accept(et); if (!et.ok) return false; return true;
  };
  if (!expr::handleListLiteral(listLiteral, out, outSet, visitChild)) { ok = false; return; }
}

