/***
 * Name: ExpressionTyper::visit(Unary)
 * Purpose: Type unary operators (neg, bitnot, not) with constraints.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Unary& u) {
  const ast::Expr* operandExpr = u.operand.get();
  if (operandExpr == nullptr) { addDiag(*diags, "null operand", &u); ok = false; return; }
  ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  operandExpr->accept(sub); if (!sub.ok) { ok = false; return; }
  const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int);
  const uint32_t fMask = TypeEnv::maskForKind(ast::TypeKind::Float);
  const uint32_t bMask = TypeEnv::maskForKind(ast::TypeKind::Bool);
  auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
  const uint32_t maskVal = (sub.outSet != 0U) ? sub.outSet : TypeEnv::maskForKind(sub.out);
  if (u.op == ast::UnaryOperator::Neg) {
    if (isSubset(maskVal, iMask)) { out = ast::TypeKind::Int; outSet = iMask; }
    else if (isSubset(maskVal, fMask)) { out = ast::TypeKind::Float; outSet = fMask; }
    else { addDiag(*diags, "unary '-' requires int or float", &u); ok = false; return; }
    auto& mu = const_cast<ast::Unary&>(u);
    mu.setType(out);
    if (u.operand) { const auto& can = u.operand->canonical(); if (can) { mu.setCanonicalKey("u:neg:(" + *can + ")"); } }
    return;
  }
  if (u.op == ast::UnaryOperator::BitNot) {
    if (!isSubset(maskVal, iMask)) { addDiag(*diags, "bitwise '~' requires int", &u); ok = false; return; }
    out = ast::TypeKind::Int; outSet = iMask;
    auto& mu = const_cast<ast::Unary&>(u);
    mu.setType(out);
    if (u.operand) { const auto& can = u.operand->canonical(); if (can) { mu.setCanonicalKey("u:bitnot:(" + *can + ")"); } }
    return;
  }
  if (!isSubset(maskVal, bMask)) { addDiag(*diags, "'not' requires bool", &u); ok = false; return; }
  out = ast::TypeKind::Bool; outSet = bMask;
  auto& mu = const_cast<ast::Unary&>(u);
  mu.setType(out);
  if (u.operand) { const auto& can2 = u.operand->canonical(); if (can2) { mu.setCanonicalKey("u:not:(" + *can2 + ")"); } }
}

