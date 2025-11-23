/***
 * Name: scanLocalsAssigned (declaration)
 * Purpose: Collect the set of local names that are assigned anywhere within a
 *          function body (including tuple/list destructuring and for-targets).
 */
#pragma once

#include <unordered_set>
#include <string>
#include "ast/FunctionDef.h"

namespace pycc::sema::detail {
    /***
     * Name: scanLocalsAssigned
     * Purpose: Populate 'out' with names that are assigned in 'fn'. This enables
     *          ExpressionTyper to flag reads of local names before assignment.
     */
    void scanLocalsAssigned(const ast::FunctionDef &fn, std::unordered_set<std::string> &out);
} // namespace pycc::sema::detail
