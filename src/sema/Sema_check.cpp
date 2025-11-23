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
#include "sema/detail/SemaImpl.h"

namespace pycc::sema {

/***
 * Name: sema_check_impl (declaration)
 * Purpose: Internal implementation referenced by Sema::check.
 */

bool Sema::check(ast::Module& mod, std::vector<Diagnostic>& diags) {
  return sema_check_impl(this, mod, diags);
}

} // namespace pycc::sema
