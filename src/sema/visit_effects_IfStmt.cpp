/***
 * Name: EffectsScan::visit(IfStmt)
 * Purpose: Scan condition and both branches for potential effects.
 */
#include "sema/detail/EffectsScan.h"
#include "ast/IfStmt.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::IfStmt& iff) {
  if (iff.cond) iff.cond->accept(*this);
  for (const auto& s: iff.thenBody) if (s) s->accept(*this);
  for (const auto& s: iff.elseBody) if (s) s->accept(*this);
}
