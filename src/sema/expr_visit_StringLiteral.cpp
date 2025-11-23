/***
 * Name: handleStringLiteral
 * Purpose: Compute typing and canonical for StringLiteral.
 */
#include "sema/detail/ExprVisitHelpers.h"
#include "sema/TypeEnv.h"
#include "ast/StringLiteral.h"

using namespace pycc;
using namespace pycc::sema;

expr::VisitResult expr::handleStringLiteral(const ast::StringLiteral& n) {
  n.setType(ast::TypeKind::Str);
  n.setCanonicalKey(std::string("s:") + std::to_string(n.value.size()));
  return expr::VisitResult{ast::TypeKind::Str, TypeEnv::maskForKind(ast::TypeKind::Str)};
}
