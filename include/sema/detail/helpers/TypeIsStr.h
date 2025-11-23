/***
 * Name: typeIsStr
 * Purpose: Predicate for Str kind.
 */
#pragma once

#include "ast/TypeKind.h"

namespace pycc::sema {
inline bool typeIsStr(ast::TypeKind t) { return t == ast::TypeKind::Str; }
}

