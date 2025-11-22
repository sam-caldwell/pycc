/***
 * Name: pycc::sema::Sema::check
 * Purpose: Entry point for semantic analysis over a Module; populates types
 *          and diagnostics, and records function flags/effects.
 * Inputs:
 *   - mod: AST module to check
 *   - diags: output diagnostics container
 * Returns:
 *   - true on success (no diagnostics), false otherwise
 */
#include "sema/Sema.h"

namespace pycc::sema {

// Forward declaration of the internal implementation (defined in Sema.cpp)
bool sema_check_impl(Sema* self, ast::Module& mod, std::vector<Diagnostic>& diags);

bool Sema::check(ast::Module& mod, std::vector<Diagnostic>& diags) {
  return sema_check_impl(this, mod, diags);
}

} // namespace pycc::sema

