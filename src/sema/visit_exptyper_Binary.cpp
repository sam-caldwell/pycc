/***
 * Name: ExpressionTyper::visit(Binary)
 * Purpose: Type-check arithmetic, bitwise, comparison, logical, membership.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Helpers.h"
#include "ast/Binary.h"

using namespace pycc;
using namespace pycc::sema;

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
void ExpressionTyper::visit(const ast::Binary& binaryNode) {
  ExpressionTyper lhsTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  ExpressionTyper rhsTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets};
  binaryNode.lhs->accept(lhsTyper);
  if (!lhsTyper.ok) { ok = false; return; }
  binaryNode.rhs->accept(rhsTyper);
  if (!rhsTyper.ok) { ok = false; return; }
  using Type = ast::TypeKind;
  // Arithmetic (incl. floor-div and pow)
  if (binaryNode.op == ast::BinaryOperator::Add || binaryNode.op == ast::BinaryOperator::Sub || binaryNode.op == ast::BinaryOperator::Mul || binaryNode.op == ast::BinaryOperator::Div || binaryNode.op == ast::BinaryOperator::Mod || binaryNode.op == ast::BinaryOperator::FloorDiv || binaryNode.op == ast::BinaryOperator::Pow) {
    const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
    const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
    const uint32_t sMask = TypeEnv::maskForKind(Type::Str);
    const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
    const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
    // String concatenation: only allow '+' for str + str
    if (binaryNode.op == ast::BinaryOperator::Add && lMask == sMask && rMask == sMask) {
      out = Type::Str; outSet = sMask; auto& mb = const_cast<ast::Binary&>(binaryNode); mb.setType(out);
      return;
    }
    if (lMask == iMask && rMask == iMask) { out = Type::Int; outSet = iMask; return; }
    if (binaryNode.op != ast::BinaryOperator::Mod && lMask == fMask && rMask == fMask) { out = Type::Float; outSet = fMask; return; }
    const uint32_t numMask = iMask | fMask;
    auto subMask = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
    if (subMask(lMask, numMask) && subMask(rMask, numMask)) { addDiag(*diags, "ambiguous numeric types; both operands must be int or both float", &binaryNode); ok = false; return; }
    addDiag(*diags, "arithmetic operands must both be int or both be float (mod only for int)", &binaryNode); ok = false; return;
  }
  // Bitwise and shifts: require ints
  if (binaryNode.op == ast::BinaryOperator::BitAnd || binaryNode.op == ast::BinaryOperator::BitOr || binaryNode.op == ast::BinaryOperator::BitXor || binaryNode.op == ast::BinaryOperator::LShift || binaryNode.op == ast::BinaryOperator::RShift) {
    const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
    const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
    const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
    if (lMask == iMask && rMask == iMask) { out = Type::Int; outSet = iMask; return; }
    addDiag(*diags, "bitwise/shift operands must be int", &binaryNode); ok = false; return;
  }
  // Comparisons
  if (binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne || binaryNode.op == ast::BinaryOperator::Lt || binaryNode.op == ast::BinaryOperator::Le || binaryNode.op == ast::BinaryOperator::Gt || binaryNode.op == ast::BinaryOperator::Ge || binaryNode.op == ast::BinaryOperator::Is || binaryNode.op == ast::BinaryOperator::IsNot) {
    // Allow eq/ne None comparisons regardless of other type
    if ((binaryNode.op == ast::BinaryOperator::Eq || binaryNode.op == ast::BinaryOperator::Ne)) {
      auto isNone = [](uint32_t m) { return m == TypeEnv::maskForKind(Type::NoneType); };
      const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
      const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
      if (isNone(lMask) || isNone(rMask)) { out = Type::Bool; outSet = TypeEnv::maskForKind(Type::Bool); return; }
    }
    // Numeric comparisons require both operands to be int or both float; string comparisons allow str/str
    const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
    const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
    const uint32_t sMask = TypeEnv::maskForKind(Type::Str);
    const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
    const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
    if (lMask == sMask && rMask == sMask) { out = Type::Bool; outSet = TypeEnv::maskForKind(Type::Bool); return; }
    if ((lMask == iMask && rMask == iMask) || (lMask == fMask && rMask == fMask)) { out = Type::Bool; outSet = TypeEnv::maskForKind(Type::Bool); return; }
    addDiag(*diags, "comparison operands must match types (int,int) or (float,float) or (str,str)", &binaryNode); ok = false; return;
  }
  // Membership
  if (binaryNode.op == ast::BinaryOperator::In || binaryNode.op == ast::BinaryOperator::NotIn) {
    const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
    const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
    const uint32_t sMask = TypeEnv::maskForKind(Type::Str);
    const uint32_t listMask = TypeEnv::maskForKind(Type::List);
    auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
    if (rMask == sMask) {
      if (!isSubset(lMask, sMask)) { addDiag(*diags, "left operand must be str when right is str for 'in'", &binaryNode); ok = false; return; }
      out = Type::Bool; outSet = TypeEnv::maskForKind(Type::Bool); return;
    }
    if (rMask == listMask || binaryNode.rhs->kind == ast::NodeKind::ListLiteral) {
      uint32_t elemMask = 0U;
      if (binaryNode.rhs->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(binaryNode.rhs.get()); elemMask = env->getListElems(nm->id); }
      else if (binaryNode.rhs->kind == ast::NodeKind::ListLiteral) {
        const auto* lst = static_cast<const ast::ListLiteral*>(binaryNode.rhs.get());
        for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
      }
      if (elemMask != 0U) {
        if (!isSubset(lMask, elemMask)) { addDiag(*diags, "left operand not permitted for membership in list", &binaryNode); ok = false; return; }
        out = Type::Bool; outSet = TypeEnv::maskForKind(Type::Bool); return;
      }
    }
    addDiag(*diags, "unsupported membership target", &binaryNode); ok = false; return;
  }
  // Logical and/or require bool operands
  if (binaryNode.op == ast::BinaryOperator::And || binaryNode.op == ast::BinaryOperator::Or) {
    const uint32_t bMask = TypeEnv::maskForKind(Type::Bool);
    const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
    const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);
    auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
    if (!isSubset(lMask, bMask) || !isSubset(rMask, bMask)) { addDiag(*diags, "logical operands must be bool", &binaryNode); ok = false; return; }
    out = Type::Bool; outSet = bMask; return;
  }
  addDiag(*diags, "unsupported binary operator", &binaryNode); ok = false; return;
}

