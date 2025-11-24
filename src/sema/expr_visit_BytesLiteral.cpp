/***
 * Name: handleBytesLiteral
 * Purpose: Compute typing and canonical for BytesLiteral.
 */
#include "sema/detail/ExprVisitHelpers.h"
#include "sema/TypeEnv.h"
#include "ast/BytesLiteral.h"

using namespace pycc;
using namespace pycc::sema;

expr::VisitResult expr::handleBytesLiteral(const ast::BytesLiteral& n) {
  n.setType(ast::TypeKind::Bytes);
  n.setCanonicalKey(std::string("b:") + std::to_string(n.value.size()));
  return expr::VisitResult{ast::TypeKind::Bytes, TypeEnv::maskForKind(ast::TypeKind::Bytes)};
}

