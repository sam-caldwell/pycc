/***
 * Name: ExpressionTyper::visit(IfExpr)
 * Purpose: Check condition is bool and branch types match.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "ast/IfExpr.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::IfExpr& ife) {
  // test must be bool
  ExpressionTyper testTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  if (ife.test) { ife.test->accept(testTyper); } else { addDiag(*diags, "if-expression missing condition", &ife); ok = false; return; }
  if (!testTyper.ok) { ok = false; return; }
  const uint32_t bMask = TypeEnv::maskForKind(ast::TypeKind::Bool);
  const uint32_t tMask = (testTyper.outSet != 0U) ? testTyper.outSet : TypeEnv::maskForKind(testTyper.out);
  auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
  if (!isSubset(tMask, bMask)) { addDiag(*diags, "if-expression condition must be bool", &ife); ok = false; return; }
  // then and else must match
  ExpressionTyper thenTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  ExpressionTyper elseTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  if (!ife.body || !ife.orelse) { addDiag(*diags, "if-expression requires both arms", &ife); ok = false; return; }
  ife.body->accept(thenTyper); if (!thenTyper.ok) { ok = false; return; }
  ife.orelse->accept(elseTyper); if (!elseTyper.ok) { ok = false; return; }
  if (thenTyper.out != elseTyper.out) { addDiag(*diags, "if-expression branches must have same type", &ife); ok = false; return; }
  out = thenTyper.out;
  outSet = (thenTyper.outSet != 0U) ? thenTyper.outSet : TypeEnv::maskForKind(out);
  auto& m = const_cast<ast::IfExpr&>(ife);
  m.setType(out);
}

