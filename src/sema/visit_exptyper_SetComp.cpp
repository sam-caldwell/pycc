/***
 * Name: ExpressionTyper::visit(SetComp)
 * Purpose: Treat set comps as List for typing; check guards.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/exptyper/CompHandlers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::SetComp &sc) {
    detail::handleSetComp(sc, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes, out, outSet, ok);
}
