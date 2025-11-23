/***
 * Name: sema_check_impl
 * Purpose: Internal implementation of semantic analysis for Sema::check.
 */
#pragma once

#include <vector>
#include <unordered_map>
#include "sema/Sema.h"
#include "sema/TypeEnv.h"
#include "sema/detail/types/Sig.h"

namespace pycc::sema {
bool sema_check_impl(Sema* self, ast::Module& mod, std::vector<Diagnostic>& diags);
}

