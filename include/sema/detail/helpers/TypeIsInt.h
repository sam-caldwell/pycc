/***
 * Name: typeIsInt
 * Purpose: Predicate for Int kind.
 */
#pragma once

#include "ast/TypeKind.h"

namespace pycc::sema {
inline bool typeIsInt(ast::TypeKind t) { return t == ast::TypeKind::Int; }
}

