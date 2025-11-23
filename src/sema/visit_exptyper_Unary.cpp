/***
 * Name: ExpressionTyper::visit(Unary)
 * Purpose: Type-check unary operations: '-', '~', 'not'.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/exptyper/UnaryHandlers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Unary &unaryNode) {
    detail::handleUnary(unaryNode, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes, out, outSet, ok);
}
