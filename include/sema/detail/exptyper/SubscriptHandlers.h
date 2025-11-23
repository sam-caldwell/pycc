/**
 * @file
 * @brief Helpers for Subscript expression typing (str/list/tuple/dict cases).
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "ast/Subscript.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"

namespace pycc::sema::detail {

bool handleSubscriptStr(const ast::Subscript& sub,
                        const TypeEnv& env,
                        const std::unordered_map<std::string, Sig>& sigs,
                        const std::unordered_map<std::string, int>& retParamIdxs,
                        std::vector<Diagnostic>& diags,
                        PolyPtrs poly,
                        const std::vector<const TypeEnv*>* outers,
                        ast::TypeKind& out,
                        uint32_t& outSet,
                        bool& ok);

bool handleSubscriptList(const ast::Subscript& sub,
                         const TypeEnv& env,
                         const std::unordered_map<std::string, Sig>& sigs,
                         const std::unordered_map<std::string, int>& retParamIdxs,
                         std::vector<Diagnostic>& diags,
                         PolyPtrs poly,
                         const std::vector<const TypeEnv*>* outers,
                         ast::TypeKind& out,
                         uint32_t& outSet,
                         bool& ok);

bool handleSubscriptTuple(const ast::Subscript& sub,
                          const TypeEnv& env,
                          const std::unordered_map<std::string, Sig>& sigs,
                          const std::unordered_map<std::string, int>& retParamIdxs,
                          std::vector<Diagnostic>& diags,
                          PolyPtrs poly,
                          const std::vector<const TypeEnv*>* outers,
                          ast::TypeKind& out,
                          uint32_t& outSet,
                          bool& ok);

bool handleSubscriptDict(const ast::Subscript& sub,
                         const TypeEnv& env,
                         const std::unordered_map<std::string, Sig>& sigs,
                         const std::unordered_map<std::string, int>& retParamIdxs,
                         std::vector<Diagnostic>& diags,
                         PolyPtrs poly,
                         const std::vector<const TypeEnv*>* outers,
                         ast::TypeKind& out,
                         uint32_t& outSet,
                         bool& ok);

} // namespace pycc::sema::detail

