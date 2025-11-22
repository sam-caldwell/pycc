/***
 * Name: ExpressionTyper::visit(StringLiteral)
 * Purpose: Type string literals and set canonical form.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>& n) {
  auto r = expr::handleStringLiteral(reinterpret_cast<const ast::StringLiteral&>(n));
  out = r.out; outSet = r.outSet;
}

