/***
 * Name: ExpressionTyper::visit(BoolLiteral)
 * Purpose: Delegate to literal helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"
#include "ast/BoolLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>& n) {
  auto r = expr::handleBoolLiteral(reinterpret_cast<const ast::BoolLiteral&>(n));
  out = r.out; outSet = r.outSet;
}

