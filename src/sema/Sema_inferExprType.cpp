/***
 * Name: inferExprType (definition)
 * Purpose: Type-check an expression and attach inferred type to the AST node.
 */
#include "sema/detail/SemaImpl.h"
#include "sema/detail/ExpressionTyper.h"

namespace pycc::sema {

bool inferExprType(const ast::Expr* expr,
                   const TypeEnv& env,
                   const std::unordered_map<std::string, Sig>& sigs,
                   const std::unordered_map<std::string, int>& retParamIdxs,
                   ast::TypeKind& outType,
                   std::vector<Diagnostic>& diags,
                   PolyPtrs poly,
                   const std::vector<const TypeEnv*>* outers,
                   const std::unordered_map<std::string, ClassInfo>* classes) {
  if (expr == nullptr) { addDiag(diags, "null expression", nullptr); return false; }
  ExpressionTyper exprTyper{env, sigs, retParamIdxs, diags, poly, outers, classes};
  expr->accept(exprTyper);
  if (!exprTyper.ok) { return false; }
  outType = exprTyper.out;
  expr->setType(outType);
  return true;
}

} // namespace pycc::sema
