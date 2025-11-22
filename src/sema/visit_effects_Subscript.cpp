/***
 * Name: EffectsScan::visit(Subscript)
 * Purpose: Subscript may raise; scan value and slice.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Subscript& s) {
  mayRaise = true;
  if (s.value) s.value->accept(*this);
  if (s.slice) s.slice->accept(*this);
}

