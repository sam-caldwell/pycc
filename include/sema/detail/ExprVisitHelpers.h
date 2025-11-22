/***
 * @file
 * @brief Helpers for ExpressionTyper literal visits, returned as VisitResult.
 */
#pragma once

#include "ast/TypeKind.h"
#include <cstdint>

#include "ast/IntLiteral.h"
#include "ast/BoolLiteral.h"
#include "ast/FloatLiteral.h"
#include "ast/StringLiteral.h"
#include "ast/NoneLiteral.h"

namespace pycc::sema::expr {

/*** @brief Minimal result for a literal visit. */
struct VisitResult { ast::TypeKind out; uint32_t outSet; };

/*** @brief Handle IntLiteral typing and canonical assignment. */
VisitResult handleIntLiteral(const ast::IntLiteral& n);
/*** @brief Handle BoolLiteral typing and canonical assignment. */
VisitResult handleBoolLiteral(const ast::BoolLiteral& n);
/*** @brief Handle FloatLiteral typing and canonical assignment. */
VisitResult handleFloatLiteral(const ast::FloatLiteral& n);
/*** @brief Handle StringLiteral typing and canonical assignment. */
VisitResult handleStringLiteral(const ast::StringLiteral& n);
/*** @brief Handle NoneLiteral typing and canonical assignment. */
VisitResult handleNoneLiteral(const ast::NoneLiteral& n);

} // namespace pycc::sema::expr
