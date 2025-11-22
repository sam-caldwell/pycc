/***
 * Name: EffectsScan::visit(ExprStmt)
 * Purpose: Scan expression statement value for potential effects.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::ExprStmt& es) { if (es.value) es.value->accept(*this); }

