/***
 * Name: ExpressionTyper::visit(BytesLiteral)
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/ExprVisitHelpers.h"

using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Literal<std::string, ast::NodeKind::BytesLiteral>& n) {
    auto r = expr::handleBytesLiteral(reinterpret_cast<const ast::BytesLiteral&>(n));
    out = r.out;
    outSet = r.outSet;
}

