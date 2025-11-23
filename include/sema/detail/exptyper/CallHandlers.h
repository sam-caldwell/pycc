/**
 * @file
 * @brief Helpers for Call expression typing.
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
 * Handle known stdlib attribute calls (math.*, subprocess.*, sys.*).
 * Returns true if handled (either success or diagnosed failure).
 */
bool handleStdLibAttributeCall(const ast::Call& callNode,
                               const TypeEnv& env,
                               const std::unordered_map<std::string, Sig>& sigs,
                               const std::unordered_map<std::string, int>& retParamIdxs,
                               std::vector<Diagnostic>& diags,
                               PolyPtrs polyTargets,
                               const std::vector<const TypeEnv*>* outers,
                               ast::TypeKind& out,
                               uint32_t& outSet,
                               bool& ok);

} // namespace pycc::sema::detail

