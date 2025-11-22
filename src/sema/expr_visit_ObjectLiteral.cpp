/***
 * Name: handleObjectLiteral
 * Purpose: Visit object fields; treat as opaque and set canonical key.
 */
#include "sema/detail/ExprVisitContainers.h"
#include "sema/TypeEnv.h"
#include "ast/ObjectLiteral.h"

using namespace pycc;
using namespace pycc::sema;

bool expr::handleObjectLiteral(const ast::ObjectLiteral& obj, ast::TypeKind& out, uint32_t& outSet,
                               const std::function<bool(const ast::Expr*)>& visitChild) {
  for (const auto& field : obj.fields) { if (!field) continue; if (!visitChild(field.get())) return false; }
  out = ast::TypeKind::NoneType; outSet = 0U;
  auto& m = const_cast<ast::ObjectLiteral&>(obj);
  m.setType(out);
  m.setCanonicalKey("obj");
  return true;
}

