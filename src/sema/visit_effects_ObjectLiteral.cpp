/***
 * Name: EffectsScan::visit(ObjectLiteral)
 * Purpose: Object literal visit for effects scanning (no-op in this subset).
 */
#include "sema/detail/EffectsScan.h"
#include "ast/ObjectLiteral.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::ObjectLiteral&) {}
