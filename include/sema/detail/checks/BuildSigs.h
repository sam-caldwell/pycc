/**
 * @file
 * @brief Build signature maps (name -> Sig) from module functions.
 */
#pragma once

#include <unordered_map>
#include <string>
#include "ast/Module.h"
#include "sema/detail/Types.h"
#include "sema/TypeEnv.h"

namespace pycc::sema::detail {

/** Build the function signature map for all top-level functions. */
void buildSigs(const ast::Module& mod,
               std::unordered_map<std::string, Sig>& outSigs);

} // namespace pycc::sema::detail

