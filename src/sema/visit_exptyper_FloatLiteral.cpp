/***
 * Name: ExpressionTyper::visit(FloatLiteral)
 * Purpose: Delegate to literal helper and set outputs.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"
#include "ast/FloatLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>& n) {
    // ReSharper disable once CppUseStructuredBinding
    const auto r = expr::handleFloatLiteral(reinterpret_cast<const ast::FloatLiteral&>(n));
  out = r.out; outSet = r.outSet;
}

