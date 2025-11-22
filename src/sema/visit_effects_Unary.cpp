/***
 * Name: EffectsScan::visit(Unary)
 * Purpose: Scan operand for potential effects.
 */
#include "sema/detail/EffectsScan.h"
#include "ast/Unary.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Unary& u) { if (u.operand) u.operand->accept(*this); }
