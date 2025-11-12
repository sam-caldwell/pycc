#pragma once

#include "ast/Literal.h"

namespace pycc::ast {

    using FloatLiteral = Literal<double, NodeKind::FloatLiteral>;

} // namespace pycc::ast

