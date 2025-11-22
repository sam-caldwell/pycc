/***
 * Name: handleListLiteral
 * Purpose: Visit list elements, set type and canonical key.
 */
#include "sema/detail/ExprVisitContainers.h"
#include "sema/TypeEnv.h"
#include "ast/ListLiteral.h"

using namespace pycc;
using namespace pycc::sema;

bool expr::handleListLiteral(const ast::ListLiteral& lst, ast::TypeKind& out, uint32_t& outSet,
                             const std::function<bool(const ast::Expr*)>& visitChild) {
  for (const auto& el : lst.elements) { if (!el) continue; if (!visitChild(el.get())) return false; }
  out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out);
  auto& mutableList = const_cast<ast::ListLiteral&>(lst);
  mutableList.setType(out);
  std::string key = "list:(";
  for (size_t i = 0; i < lst.elements.size(); ++i) {
    const auto& element = lst.elements[i];
    if (i > 0) key += ",";
    if (element && element->canonical()) key += *element->canonical(); else key += "?";
  }
  key += ")";
  mutableList.setCanonicalKey(key);
  return true;
}

