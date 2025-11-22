/***
 * Name: EffectsScan::visit(Attribute)
 * Purpose: Attribute access may raise; scan base value.
 */
#include "sema/detail/EffectsScan.h"
#include "ast/Attribute.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Attribute& a) {
  mayRaise = true;
  if (a.value) a.value->accept(*this);
}
