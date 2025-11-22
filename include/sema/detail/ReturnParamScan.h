/***
 * @file
 * @brief Interprocedural scan for functions that trivially return a parameter.
 */
#pragma once

#include <unordered_map>
#include <string>
#include "ast/Module.h"

namespace pycc::sema {

/***
 * @brief Build a map: function name -> parameter index that is always returned.
 * If a function returns different params or non-names, it is omitted.
 */
std::unordered_map<std::string, int> computeReturnParamIdxs(const ast::Module& mod);

} // namespace pycc::sema

