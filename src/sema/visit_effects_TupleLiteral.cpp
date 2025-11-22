/***
 * Name: EffectsScan::visit(TupleLiteral)
 * Purpose: Scan each tuple element for effects.
 */
#include "sema/detail/EffectsScan.h"
#include "ast/TupleLiteral.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::TupleLiteral& t) { for (const auto& el : t.elements) if (el) el->accept(*this); }
