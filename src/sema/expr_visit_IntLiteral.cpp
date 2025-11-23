/***
 * Name: handleIntLiteral
 * Purpose: Compute typing and canonical for IntLiteral.
 */
#include "sema/detail/ExprVisitHelpers.h"
#include "sema/TypeEnv.h"
#include "ast/IntLiteral.h"

using namespace pycc;
using namespace pycc::sema;

expr::VisitResult expr::handleIntLiteral(const ast::IntLiteral& n) {
  n.setType(ast::TypeKind::Int);
  n.setCanonicalKey(std::string("i:") + std::to_string(static_cast<long long>(n.value)));
  return expr::VisitResult{ast::TypeKind::Int, TypeEnv::maskForKind(ast::TypeKind::Int)};
}
