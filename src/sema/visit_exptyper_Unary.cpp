/***
 * Name: ExpressionTyper::visit(Unary)
 * Purpose: Type-check unary operations: '-', '~', 'not'.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "ast/Unary.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Unary& unaryNode) {
  const ast::Expr* operandExpr = unaryNode.operand.get();
  if (operandExpr == nullptr) { addDiag(*diags, "null operand", &unaryNode); ok = false; return; }
  ExpressionTyper sub{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  operandExpr->accept(sub); if (!sub.ok) { ok = false; return; }
  const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int);
  const uint32_t fMask = TypeEnv::maskForKind(ast::TypeKind::Float);
  const uint32_t bMask = TypeEnv::maskForKind(ast::TypeKind::Bool);
  auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
  const uint32_t maskVal = (sub.outSet != 0U) ? sub.outSet : TypeEnv::maskForKind(sub.out);
  if (unaryNode.op == ast::UnaryOperator::Neg) {
    if (isSubset(maskVal, iMask)) { out = ast::TypeKind::Int; outSet = iMask; }
    else if (isSubset(maskVal, fMask)) { out = ast::TypeKind::Float; outSet = fMask; }
    else { addDiag(*diags, "unary '-' requires int or float", &unaryNode); ok = false; return; }
    auto& mutableUnary = const_cast<ast::Unary&>(unaryNode); // NOLINT
    mutableUnary.setType(out);
    if (unaryNode.operand) { const auto& can = unaryNode.operand->canonical(); if (can) { mutableUnary.setCanonicalKey("u:neg:(" + *can + ")"); } }
    return;
  }
  if (unaryNode.op == ast::UnaryOperator::BitNot) {
    if (!isSubset(maskVal, iMask)) { addDiag(*diags, "bitwise '~' requires int", &unaryNode); ok = false; return; }
    out = ast::TypeKind::Int; outSet = iMask;
    auto& mutableUnaryBN = const_cast<ast::Unary&>(unaryNode);
    mutableUnaryBN.setType(out);
    if (unaryNode.operand) { const auto& can = unaryNode.operand->canonical(); if (can) { mutableUnaryBN.setCanonicalKey("u:bitnot:(" + *can + ")"); } }
    return;
  }
  // 'not'
  if (!isSubset(maskVal, bMask)) { addDiag(*diags, "'not' requires bool", &unaryNode); ok = false; return; }
  out = ast::TypeKind::Bool; outSet = bMask;
  auto& mutableUnary2 = const_cast<ast::Unary&>(unaryNode); // NOLINT
  mutableUnary2.setType(out);
  if (unaryNode.operand) { const auto& can2 = unaryNode.operand->canonical(); if (can2) { mutableUnary2.setCanonicalKey("u:not:(" + *can2 + ")"); } }
}

