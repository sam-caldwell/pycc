/**
 * @file
 * @brief Name resolution handler for ExpressionTyper.
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "ast/Name.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/Diagnostic.h"

namespace pycc::sema::detail {

void handleNameResolve(const ast::Name& n,
                       const TypeEnv& env,
                       const std::vector<const TypeEnv*>* outers,
                       std::vector<Diagnostic>& diags,
                       ast::TypeKind& out,
                       uint32_t& outSet,
                       bool& ok);

} // namespace pycc::sema::detail

