#pragma once

#include "ast/Literal.h"

namespace pycc::ast {

    using IntLiteral = Literal<int64_t, NodeKind::IntLiteral>;

} // namespace pycc::ast
