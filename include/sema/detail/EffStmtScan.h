/***
 * @file
 * @brief Scan statements for may-raise effects using EffectsScan.
 */
#pragma once

#include <unordered_map>
#include "ast/Module.h"
#include "ast/VisitorBase.h"
#include "ast/Nodes.h"
#include "sema/detail/EffectsScan.h"

namespace pycc::sema {
    /***
     * @brief Populate out map with per-statement mayRaise flags across all functions in module.
     */
    void scanStmtEffects(const ast::Module &mod,
                         std::unordered_map<const ast::Stmt *, bool> &out);
} // namespace pycc::sema
