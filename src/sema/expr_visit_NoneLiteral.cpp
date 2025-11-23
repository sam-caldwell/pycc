/***
 * Name: handleNoneLiteral
 * Purpose: Compute typing and canonical for NoneLiteral.
 */
#include "sema/detail/ExprVisitHelpers.h"
#include "sema/TypeEnv.h"
#include "ast/NoneLiteral.h"

using namespace pycc;
using namespace pycc::sema;

expr::VisitResult expr::handleNoneLiteral(const ast::NoneLiteral& n) {
  n.setType(ast::TypeKind::NoneType);
  n.setCanonicalKey("none");
  return expr::VisitResult{ast::TypeKind::NoneType, TypeEnv::maskForKind(ast::TypeKind::NoneType)};
}
