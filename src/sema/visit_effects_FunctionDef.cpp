/***
 * Name: EffectsScan::visit(FunctionDef)
 * Purpose: Function definition does not contribute to may-raise effect here.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::FunctionDef&) {}

