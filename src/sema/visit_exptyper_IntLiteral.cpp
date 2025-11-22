/***
 * Name: ExpressionTyper::visit(IntLiteral)
 * Purpose: Delegate to literal helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"
#include "ast/IntLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>& n) {
  auto r = expr::handleIntLiteral(reinterpret_cast<const ast::IntLiteral&>(n));
  out = r.out; outSet = r.outSet;
}

