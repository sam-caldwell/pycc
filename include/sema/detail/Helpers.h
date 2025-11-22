/***
 * @file
 * @brief Internal Sema helper utilities (diagnostics, simple type predicates).
 */
#pragma once

#include "sema/Sema.h"
#include "ast/Node.h"
#include "ast/TypeKind.h"
#include <vector>
#include <string>

namespace pycc::sema {

using Type = ast::TypeKind;

/***
 * @brief Append a diagnostic message (with optional source node location).
 */
void addDiag(std::vector<Diagnostic>& diags, const std::string& msg, const ast::Node* n);

/*** @brief Predicate: is Int kind. */
inline bool typeIsInt(Type t) { return t == Type::Int; }
/*** @brief Predicate: is Bool kind. */
inline bool typeIsBool(Type t) { return t == Type::Bool; }
/*** @brief Predicate: is Float kind. */
inline bool typeIsFloat(Type t) { return t == Type::Float; }
/*** @brief Predicate: is Str kind. */
inline bool typeIsStr(Type t) { return t == Type::Str; }

} // namespace pycc::sema
