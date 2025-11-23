/**
 * @file
 * @brief Validate special (dunder) class methods for required arity/return types.
 */
#pragma once

#include <string>
#include <vector>
#include "sema/Diagnostic.h"
#include "sema/detail/Types.h"
#include "ast/FunctionDef.h"

namespace pycc::sema::detail {

/** Validate magic methods like __init__, __len__, __get__, etc. */
void validateClassMethod(const ast::FunctionDef* fn,
                         const std::string& className,
                         std::vector<Diagnostic>& diags);

} // namespace pycc::sema::detail

