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
  n.setType(ast::TypeKind::Float);
  constexpr int kDoublePrecision = 17;
  std::ostringstream oss;
  oss.setf(static_cast<std::ios::fmtflags>(0), std::ios::floatfield);
  oss.precision(kDoublePrecision);
  oss << n.value;
  n.setCanonicalKey(std::string("f:") + oss.str());
  return expr::VisitResult{ast::TypeKind::Float, TypeEnv::maskForKind(ast::TypeKind::Float)};
}
