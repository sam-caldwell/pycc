/**
 * @file
 * @brief Helpers to resolve function and attribute calls against signatures.
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "ast/Call.h"
#include "ast/Attribute.h"
#include "ast/Name.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"

namespace pycc::sema::detail {

bool resolveNamedCall(const ast::Call& callNode,
                      const ast::Name* calleeName,
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

bool resolveAttributeCall(const ast::Call& callNode,
                          const ast::Attribute* at,
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

