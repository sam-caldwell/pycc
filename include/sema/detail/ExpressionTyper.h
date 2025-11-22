/***
 * @file
 * @brief Forward declaration of ExpressionTyper visitor used for expression typing.
 */
#pragma once

namespace pycc::sema {

/***
 * @class ExpressionTyper
 * @brief AST visitor that infers types and canonical forms for expressions.
 *
 * The full definition and most visit methods remain in the implementation
 * translation units while we split functionality incrementally. This header
 * provides a declaration for use by other components.
 */
class ExpressionTyper; // definition in Sema implementation units

} // namespace pycc::sema

