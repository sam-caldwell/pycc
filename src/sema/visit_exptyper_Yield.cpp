/***
 * Name: ExpressionTyper::visit(YieldExpr)
 * Purpose: Conservative typing for 'yield' in this subset.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::YieldExpr& y) {
    (void)y; addDiag(*diags, "'yield' not supported in this subset", &y); ok = false;
}

