/***
 * Name: EffectsScan::visit(NoneLiteral)
 * Purpose: None literal cannot raise.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::NoneLiteral&) {}

