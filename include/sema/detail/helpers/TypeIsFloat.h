/***
 * Name: typeIsFloat
 * Purpose: Predicate for Float kind.
 */
#pragma once

#include "ast/TypeKind.h"

namespace pycc::sema {
inline bool typeIsFloat(ast::TypeKind t) { return t == ast::TypeKind::Float; }
}

