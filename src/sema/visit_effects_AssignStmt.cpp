/***
 * Name: EffectsScan::visit(AssignStmt)
 * Purpose: Scan assignment value for potential effects.
 */
#include "sema/detail/EffectsScan.h"
#include "ast/AssignStmt.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::AssignStmt& as) { if (as.value) as.value->accept(*this); }
