/***
 * Name: ExpressionTyper::visit(NoneLiteral)
 * Purpose: Type None literal and set canonical form.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::NoneLiteral& n) {
  auto r = expr::handleNoneLiteral(n);
  out = r.out; outSet = r.outSet;
}

