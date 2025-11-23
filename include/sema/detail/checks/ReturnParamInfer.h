/**
 * @file
 * @brief Helper to infer returned parameter index for a function body.
 */
#pragma once

#include <optional>
#include "ast/FunctionDef.h"

namespace pycc::sema::detail {

/**
 * Inspect a function body and determine if it consistently returns one of
 * its parameters. Returns the parameter index when consistent, or std::nullopt.
 */
std::optional<int> inferReturnParamIdx(const ast::FunctionDef& fn);

} // namespace pycc::sema::detail

