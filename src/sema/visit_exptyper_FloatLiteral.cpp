/***
 * Name: ExpressionTyper::visit(FloatLiteral)
 * Purpose: Type float literals and set canonical form.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>& n) {
  auto r = expr::handleFloatLiteral(reinterpret_cast<const ast::FloatLiteral&>(n));
  out = r.out; outSet = r.outSet;
}

