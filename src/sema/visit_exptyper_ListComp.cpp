/***
 * Name: ExpressionTyper::visit(ListComp)
 * Purpose: Type-check list comprehensions with local env and bool guards.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/exptyper/CompHandlers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::ListComp &lc) {
    detail::handleListComp(lc, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes, out, outSet, ok);
}
