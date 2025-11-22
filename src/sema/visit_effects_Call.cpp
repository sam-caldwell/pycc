/***
 * Name: EffectsScan::visit(Call)
 * Purpose: Calls may raise; scan callee and arguments.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Call& c) {
  mayRaise = true;
  if (c.callee) c.callee->accept(*this);
  for (const auto& a : c.args) if (a) a->accept(*this);
}

