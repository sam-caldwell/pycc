/***
 * Name: EffectsScan::visit(ReturnStmt)
 * Purpose: Scan return value for potential effects.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::ReturnStmt& r) { if (r.value) r.value->accept(*this); }

