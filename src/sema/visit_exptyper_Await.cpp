/***
 * Name: ExpressionTyper::visit(AwaitExpr)
 * Purpose: Conservative typing for 'await' in this subset.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::AwaitExpr& a) {
  (void)a; addDiag(*diags, "'await' not supported in this subset", &a); ok = false;
}

