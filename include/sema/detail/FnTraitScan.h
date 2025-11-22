/***
 * @file
 * @brief Pre-scan functions for generator/coroutine traits (yield/await).
 */
#pragma once

#include <unordered_map>
#include "ast/Module.h"
#include "sema/Sema.h"

namespace pycc::sema {

/***
 * @brief Populate function flags map for the module.
 */
void scanFunctionTraits(const ast::Module& mod,
                        std::unordered_map<const ast::FunctionDef*, Sema::FuncFlags>& out);

} // namespace pycc::sema

