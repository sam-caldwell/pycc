/***
 * Name: handleFloatLiteral
 * Purpose: Compute typing and canonical for FloatLiteral.
 */
#include "sema/detail/ExprVisitHelpers.h"
#include "sema/TypeEnv.h"
#include "ast/FloatLiteral.h"
#include <sstream>
#include <ios>

using namespace pycc;
using namespace pycc::sema;

expr::VisitResult expr::handleFloatLiteral(const ast::FloatLiteral& n) {
  auto& mutableFloat = const_cast<ast::FloatLiteral&>(n);
  mutableFloat.setType(ast::TypeKind::Float);
  constexpr int kDoublePrecision = 17;
  std::ostringstream oss;
  oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
  oss.precision(kDoublePrecision);
  oss << n.value;
  mutableFloat.setCanonicalKey(std::string("f:") + oss.str());
  return expr::VisitResult{ast::TypeKind::Float, TypeEnv::maskForKind(ast::TypeKind::Float)};
}

