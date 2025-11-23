/***
 * Name: handleTupleLiteral
 * Purpose: Visit tuple elements, set type and canonical key.
 */
#include "sema/detail/ExprVisitContainers.h"
#include "sema/TypeEnv.h"
#include "ast/TupleLiteral.h"

using namespace pycc;
using namespace pycc::sema;

bool expr::handleTupleLiteral(const ast::TupleLiteral& tup, ast::TypeKind& out, uint32_t& outSet,
                              const std::function<bool(const ast::Expr*)>& visitChild) {
  for (const auto& el : tup.elements) { if (!el) continue; if (!visitChild(el.get())) return false; }
  out = ast::TypeKind::Tuple; outSet = TypeEnv::maskForKind(out);
  tup.setType(out);
  std::string key = "tuple:(";
  for (size_t i = 0; i < tup.elements.size(); ++i) {
    const auto& element = tup.elements[i];
    if (i > 0) key += ",";
    if (element && element->canonical()) key += *element->canonical(); else key += "?";
  }
  key += ")";
  tup.setCanonicalKey(key);
  return true;
}
