/***
 * Name: typeIsBool
 * Purpose: Predicate for Bool kind.
 */
#pragma once

#include "ast/TypeKind.h"

namespace pycc::sema {
inline bool typeIsBool(ast::TypeKind t) { return t == ast::TypeKind::Bool; }
}

