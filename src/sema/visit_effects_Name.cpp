/***
 * Name: EffectsScan::visit(Name)
 * Purpose: Name access does not imply raising by itself here.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Name&) {}

