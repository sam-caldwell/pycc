/***
 * Name: ExpressionTyper::visit(DictComp)
 * Purpose: Type-check dict comprehensions; require bool guards and model element masks.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/exptyper/CompHandlers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::DictComp &dc) {
    detail::handleDictComp(dc, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes, out, outSet, ok);
}
