/**
 * @file
 * @brief Collect class info (bases + method sigs) and validate special methods.
 */
#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include "ast/Module.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"

namespace pycc::sema::detail {

/** Collect classes map with bases and methods, emitting diagnostics for magic methods. */
void collectClasses(const ast::Module& mod,
                    std::unordered_map<std::string, ClassInfo>& out,
                    std::vector<Diagnostic>& diags);

} // namespace pycc::sema::detail

