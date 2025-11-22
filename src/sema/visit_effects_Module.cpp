/***
 * Name: EffectsScan::visit(Module)
 * Purpose: Module visit is a no-op for effects scanning.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Module&) {}

