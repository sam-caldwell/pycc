/**
 * @file
 * @brief Builtin call handlers (len, eval/exec, obj_get, etc.).
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "ast/Call.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"

namespace pycc::sema::detail {

/**
 * Handle builtin name calls with special typing rules.
 * Returns true if handled (either success or diagnosed failure).
 */
bool handleBuiltinCall(const ast::Call& callNode,
                       const TypeEnv& env,
                       const std::unordered_map<std::string, Sig>& sigs,
                       const std::unordered_map<std::string, int>& retParamIdxs,
                       std::vector<Diagnostic>& diags,
                       PolyPtrs polyTargets,
                       ast::TypeKind& out,
                       uint32_t& outSet,
                       bool& ok);

} // namespace pycc::sema::detail

