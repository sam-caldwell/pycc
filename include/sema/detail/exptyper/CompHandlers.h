/**
 * @file
 * @brief Comprehension typing handlers for list/set/dict/generator expressions.
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "ast/Nodes.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"

namespace pycc::sema::detail {

bool handleListComp(const ast::ListComp& lc,
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

bool handleSetComp(const ast::SetComp& sc,
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

bool handleDictComp(const ast::DictComp& dc,
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

bool handleGeneratorExpr(const ast::GeneratorExpr& ge,
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

