/**
 * @file
 * @brief Helpers to handle Binary expression typing, each small and focused.
 */
#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "ast/Binary.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"
#include "sema/detail/helpers/AddDiag.h"

namespace pycc::sema::detail {

/**
 * Handle arithmetic (+, -, *, /, //, %, **). Returns true if handled
 * and sets out/outSet; otherwise leaves them unchanged and returns false.
 */
bool handleBinaryArithmetic(const ast::Binary& node,
                            uint32_t lMask,
                            uint32_t rMask,
                            ast::TypeKind& out,
                            uint32_t& outSet,
                            std::vector<Diagnostic>& diags);

/**
 * Handle bitwise and shifts (&, |, ^, <<, >>). Returns true if handled.
 */
bool handleBinaryBitwise(const ast::Binary& node,
                         uint32_t lMask,
                         uint32_t rMask,
                         ast::TypeKind& out,
                         uint32_t& outSet,
                         std::vector<Diagnostic>& diags);

/**
 * Handle comparisons (==, !=, <, <=, >, >=, is, is not). Returns true if handled.
 */
bool handleBinaryComparison(const ast::Binary& node,
                            uint32_t lMask,
                            uint32_t rMask,
                            ast::TypeKind& out,
                            uint32_t& outSet,
                            std::vector<Diagnostic>& diags);

/**
 * Handle membership (in, not in). Returns true if handled.
 * Requires env and the original left/right expressions to inspect list elements.
 */
bool handleBinaryMembership(const ast::Binary& node,
                            uint32_t lMask,
                            uint32_t rMask,
                            const TypeEnv& env,
                            const std::unordered_map<std::string, Sig>& sigs,
                            const std::unordered_map<std::string, int>& retParamIdxs,
                            std::vector<Diagnostic>& diags,
                            PolyPtrs poly,
                            const std::vector<const TypeEnv*>* outers,
                            ast::TypeKind& out,
                            uint32_t& outSet);

/**
 * Handle logical (and, or). Returns true if handled.
 */
bool handleBinaryLogical(const ast::Binary& node,
                         uint32_t lMask,
                         uint32_t rMask,
                         ast::TypeKind& out,
                         uint32_t& outSet,
                         std::vector<Diagnostic>& diags);

} // namespace pycc::sema::detail
