/***
 * Name: EffectsScan::visit(ListLiteral)
 * Purpose: Scan each list element for effects.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::ListLiteral& l) { for (const auto& el : l.elements) if (el) el->accept(*this); }

