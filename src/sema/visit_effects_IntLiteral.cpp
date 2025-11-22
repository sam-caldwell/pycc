/***
 * Name: EffectsScan::visit(IntLiteral)
 * Purpose: Integer literal cannot raise.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>&) {}
