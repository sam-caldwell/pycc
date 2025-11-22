/***
 * Name: ExpressionTyper::visit(IntLiteral)
 * Purpose: Type int literals and set canonical form.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>& n) {
  auto r = expr::handleIntLiteral(reinterpret_cast<const ast::IntLiteral&>(n));
  out = r.out; outSet = r.outSet;
}

