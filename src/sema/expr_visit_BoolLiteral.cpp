/***
 * Name: handleBoolLiteral
 * Purpose: Compute typing and canonical for BoolLiteral.
 */
#include "sema/detail/ExprVisitHelpers.h"
#include "sema/TypeEnv.h"
#include "ast/BoolLiteral.h"

using namespace pycc;
using namespace pycc::sema;

expr::VisitResult expr::handleBoolLiteral(const ast::BoolLiteral& n) {
  n.setType(ast::TypeKind::Bool);
  n.setCanonicalKey(std::string("b:") + (n.value ? "1" : "0"));
  return expr::VisitResult{ast::TypeKind::Bool, TypeEnv::maskForKind(ast::TypeKind::Bool)};
}
