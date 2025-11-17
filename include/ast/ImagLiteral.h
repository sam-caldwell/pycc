#pragma once

#include "ast/Literal.h"

namespace pycc::ast {

// Imaginary numeric literal: value is the imaginary part (real=0)
using ImagLiteral = Literal<double, NodeKind::ImagLiteral>;

} // namespace pycc::ast

