/***
 * @file
 * @brief Pre-scan functions for generator/coroutine traits (yield/await).
 */
#pragma once

#include <unordered_map>
#include "ast/Module.h"
#include "ast/VisitorBase.h"
#include "ast/Nodes.h"
#include "sema/FuncFlags.h"

namespace pycc::sema {
    /***
     * @brief Populate the function flags map for the module.
     */
    void scanFunctionTraits(const ast::Module &mod,
                            std::unordered_map<const ast::FunctionDef *, FuncFlags> &out);
} // namespace pycc::sema
