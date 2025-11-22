/***
 * Name: ExpressionTyper::visit(NoneLiteral)
 * Purpose: Delegate to literal helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"
#include "ast/NoneLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::NoneLiteral& n) {
  auto r = expr::handleNoneLiteral(n);
  out = r.out; outSet = r.outSet;
}

