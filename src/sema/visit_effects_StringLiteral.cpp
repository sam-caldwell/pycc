/***
 * Name: EffectsScan::visit(StringLiteral)
 * Purpose: String literal cannot raise.
 */
#include "sema/detail/EffectsScan.h"

using namespace pycc;
using pycc::sema::EffectsScan;

void EffectsScan::visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>&) {}
