/***
 * Name: ExpressionTyper::visit(Binary)
 * Purpose: Type-check arithmetic, bitwise, comparison, logical, membership.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Helpers.h"
#include "ast/Binary.h"
#include "sema/detail/exptyper/BinaryHandlers.h"

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
  const uint32_t lMask = (lhsTyper.outSet != 0U) ? lhsTyper.outSet : TypeEnv::maskForKind(lhsTyper.out);
  const uint32_t rMask = (rhsTyper.outSet != 0U) ? rhsTyper.outSet : TypeEnv::maskForKind(rhsTyper.out);

  using detail::handleBinaryArithmetic;
  using detail::handleBinaryBitwise;
  using detail::handleBinaryComparison;
  using detail::handleBinaryMembership;
  using detail::handleBinaryLogical;

  if (handleBinaryArithmetic(binaryNode, lMask, rMask, out, outSet, *diags)) { if (outSet) return; if (!ok) return; if (binaryNode.op == ast::BinaryOperator::Add && out == ast::TypeKind::Str) { binaryNode.setType(out); } return; }
  if (handleBinaryBitwise(binaryNode, lMask, rMask, out, outSet, *diags)) { if (outSet) return; ok = false; return; }
  if (handleBinaryComparison(binaryNode, lMask, rMask, out, outSet, *diags)) { if (outSet) return; ok = false; return; }
  if (handleBinaryMembership(binaryNode, lMask, rMask, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, out, outSet)) { if (outSet) return; ok = false; return; }
  if (handleBinaryLogical(binaryNode, lMask, rMask, out, outSet, *diags)) { if (outSet) return; ok = false; return; }
  addDiag(*diags, "unsupported binary operator", &binaryNode); ok = false; return;
}
