/***
 * Name: ExpressionTyper::visit(Binary)
 * Purpose: Type binary operators (arith/bitwise/compare) with constraints.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Binary& b) {
  ExpressionTyper lhs{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  ExpressionTyper rhs{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  b.lhs->accept(lhs); if (!lhs.ok) { ok = false; return; }
  b.rhs->accept(rhs); if (!rhs.ok) { ok = false; return; }
  const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int);
  const uint32_t fMask = TypeEnv::maskForKind(ast::TypeKind::Float);
  const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
  const uint32_t lMask = (lhs.outSet != 0U) ? lhs.outSet : TypeEnv::maskForKind(lhs.out);
  const uint32_t rMask = (rhs.outSet != 0U) ? rhs.outSet : TypeEnv::maskForKind(rhs.out);
  auto subMask = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
  // Arithmetic
  if (b.op == ast::BinaryOperator::Add || b.op == ast::BinaryOperator::Sub || b.op == ast::BinaryOperator::Mul || b.op == ast::BinaryOperator::Div || b.op == ast::BinaryOperator::Mod || b.op == ast::BinaryOperator::FloorDiv || b.op == ast::BinaryOperator::Pow) {
    if (b.op == ast::BinaryOperator::Add && lMask == sMask && rMask == sMask) { out = ast::TypeKind::Str; outSet = sMask; auto& mb = const_cast<ast::Binary&>(b); mb.setType(out); return; }
    if (lMask == iMask && rMask == iMask) { out = ast::TypeKind::Int; outSet = iMask; return; }
    if (b.op != ast::BinaryOperator::Mod && lMask == fMask && rMask == fMask) { out = ast::TypeKind::Float; outSet = fMask; return; }
    const uint32_t numMask = iMask | fMask;
    if (subMask(lMask, numMask) && subMask(rMask, numMask)) { addDiag(*diags, "ambiguous numeric types; both operands must be int or both float", &b); ok = false; return; }
    addDiag(*diags, "arithmetic operands must both be int or both be float", &b); ok = false; return;
  }
  // Bitwise and shifts
  if (b.op == ast::BinaryOperator::BitAnd || b.op == ast::BinaryOperator::BitOr || b.op == ast::BinaryOperator::BitXor || b.op == ast::BinaryOperator::LShift || b.op == ast::BinaryOperator::RShift) {
    if (lMask == iMask && rMask == iMask) { out = ast::TypeKind::Int; outSet = iMask; return; }
    addDiag(*diags, "bitwise/shift operands must be int", &b); ok = false; return;
  }
  // Comparisons
  if (b.op == ast::BinaryOperator::Eq || b.op == ast::BinaryOperator::Ne || b.op == ast::BinaryOperator::Lt || b.op == ast::BinaryOperator::Le || b.op == ast::BinaryOperator::Gt || b.op == ast::BinaryOperator::Ge || b.op == ast::BinaryOperator::Is || b.op == ast::BinaryOperator::IsNot) {
    if ((b.op == ast::BinaryOperator::Eq || b.op == ast::BinaryOperator::Ne || b.op == ast::BinaryOperator::Is || b.op == ast::BinaryOperator::IsNot) && (b.lhs->kind == ast::NodeKind::NoneLiteral || b.rhs->kind == ast::NodeKind::NoneLiteral)) { out = ast::TypeKind::Bool; auto& mb = const_cast<ast::Binary&>(b); mb.setType(out); return; }
    const bool bothStr = (lMask == sMask) && (rMask == sMask);
    if (b.op == ast::BinaryOperator::Is || b.op == ast::BinaryOperator::IsNot) { out = ast::TypeKind::Bool; auto& mb = const_cast<ast::Binary&>(b); mb.setType(out); return; }
    if (bothStr) { out = ast::TypeKind::Bool; auto& mb = const_cast<ast::Binary&>(b); mb.setType(out); return; }
    if ((lMask == iMask && rMask == iMask) || (lMask == fMask && rMask == fMask)) { out = ast::TypeKind::Bool; auto& mb = const_cast<ast::Binary&>(b); mb.setType(out); return; }
    addDiag(*diags, "unsupported comparison operand types", &b); ok = false; return;
  }
  addDiag(*diags, "unsupported binary op", &b); ok = false;
}

