/***
 * Name: inferExprType
 * Purpose: Type-check a single expression and attach inferred type.
 */
#pragma once

#include <vector>
#include <unordered_map>
#include "sema/TypeEnv.h"
#include "sema/detail/types/Sig.h"
#include "sema/detail/types/ClassInfo.h"
#include "sema/detail/types/PolyPtrs.h"

namespace pycc::sema {
bool inferExprType(const ast::Expr* expr,
                   const TypeEnv& env,
                   const std::unordered_map<std::string, Sig>& sigs,
                   const std::unordered_map<std::string, int>& retParamIdxs,
                   ast::TypeKind& outType,
                   std::vector<Diagnostic>& diags,
                   PolyPtrs poly = {},
                   const std::vector<const TypeEnv*>* outers = nullptr,
                   const std::unordered_map<std::string, ClassInfo>* classes = nullptr);
}

