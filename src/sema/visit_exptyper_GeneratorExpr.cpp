/***
 * Name: ExpressionTyper::visit(GeneratorExpr)
 * Purpose: Conservative typing for generator expressions in this subset.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/exptyper/CompHandlers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::GeneratorExpr& ge) {
  detail::handleGeneratorExpr(ge, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes, out, outSet, ok);
}
