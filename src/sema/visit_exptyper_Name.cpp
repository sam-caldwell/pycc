/***
 * Name: ExpressionTyper::visit(Name)
 * Purpose: Resolve name types from current and outer environments with locals enforcement.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/LocalsAssigned.h"
#include "sema/TypeEnv.h"
#include "sema/detail/exptyper/NameHandlers.h"
#include "sema/detail/LocalsAssigned.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Name &n) {
    // Respect locals: error on read-before-assign for local-only names
    if (pycc::sema::detail::g_locals_assigned && pycc::sema::detail::g_locals_assigned->contains(n.id) && env->getSet(n.id) == 0U) {
        addDiag(*diags, std::string("local variable referenced before assignment: ") + n.id, &n);
        ok = false; return;
    }
    detail::handleNameResolve(n, *env, outers, *diags, out, outSet, ok);
}
