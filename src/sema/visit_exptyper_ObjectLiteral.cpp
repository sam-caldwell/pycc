/***
 * Name: ExpressionTyper::visit(ObjectLiteral)
 * Purpose: Delegate to container helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitContainers.h"
#include "ast/ObjectLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::ObjectLiteral& obj) {
  auto visitChild = [&](const ast::Expr* e){
    ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets};
    e->accept(et); if (!et.ok) return false; return true;
  };
  if (!expr::handleObjectLiteral(obj, out, outSet, visitChild)) { ok = false; return; }
}

