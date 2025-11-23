/***
 * Name: ExpressionTyper::visit(DictLiteral)
 * Purpose: Validate keys/values and type as Dict for subset.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "ast/DictLiteral.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::DictLiteral &dictLiteral) {
    // ReSharper disable once CppUseStructuredBinding
    for (const auto &kv: dictLiteral.items) {
        if (kv.first) {
            ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets};
            kv.first->accept(kt);
            if (!kt.ok) {
                ok = false;
                return;
            }
        }
        if (kv.second) {
            ExpressionTyper vt{*env, *sigs, *retParamIdxs, *diags, polyTargets};
            kv.second->accept(vt);
            if (!vt.ok) {
                ok = false;
                return;
            }
        }
    }
    for (const auto &up: dictLiteral.unpacks) {
        if (up) {
            ExpressionTyper ut{*env, *sigs, *retParamIdxs, *diags, polyTargets};
            up->accept(ut);
            if (!ut.ok) {
                ok = false;
                return;
            }
        }
    }
    out = ast::TypeKind::Dict;
    outSet = TypeEnv::maskForKind(out);
}
