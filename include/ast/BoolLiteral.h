#pragma once

#include "ast/Literal.h"

namespace pycc::ast {

    using BoolLiteral = Literal<bool, NodeKind::BoolLiteral>;

} // namespace pycc::ast
