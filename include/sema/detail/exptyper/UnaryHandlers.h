/**
 * @file
 * @brief Unary operator typing handler for ExpressionTyper.
 */
#pragma once

#include "ast/Unary.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"

namespace pycc::sema::detail {

void handleUnary(const ast::Unary& node,
                 const TypeEnv& env,
                 const std::unordered_map<std::string, Sig>& sigs,
                 const std::unordered_map<std::string, int>& retParamIdxs,
                 std::vector<Diagnostic>& diags,
                 PolyPtrs polyTargets,
                 const std::vector<const TypeEnv*>* outers,
                 const std::unordered_map<std::string, ClassInfo>* classes,
                 ast::TypeKind& out,
                 uint32_t& outSet,
                 bool& ok);

} // namespace pycc::sema::detail

