/***
 * Name: EffectsScan::visit(Binary)
 * Purpose: Division/modulo may raise; scan operands.
 */
#include "sema/detail/EffectsScan.h"
#include "ast/BinaryOperator.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Binary& b) {
  using BO = ast::BinaryOperator;
  if (b.op == BO::Div || b.op == BO::Mod) mayRaise = true;
  if (b.lhs) b.lhs->accept(*this);
  if (b.rhs) b.rhs->accept(*this);
}

