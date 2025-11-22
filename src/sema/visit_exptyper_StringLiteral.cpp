/***
 * Name: ExpressionTyper::visit(StringLiteral)
 * Purpose: Delegate to literal helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"
#include "ast/StringLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>& n) {
  auto r = expr::handleStringLiteral(reinterpret_cast<const ast::StringLiteral&>(n));
  out = r.out; outSet = r.outSet;
}

