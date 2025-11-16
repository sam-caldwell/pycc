#pragma once

#include <string>
#include "ast/Literal.h"

namespace pycc::ast {

// Represent bytes as std::string of raw bytes (no encoding assumptions)
using BytesLiteral = Literal<std::string, NodeKind::BytesLiteral>;

} // namespace pycc::ast

