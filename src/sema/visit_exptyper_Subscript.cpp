/***
 * Name: ExpressionTyper::visit(Subscript)
 * Purpose: Type-check value[index] for str/list/tuple/dict.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Helpers.h"
#include "ast/Subscript.h"
#include "ast/Name.h"
#include "ast/ListLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/DictLiteral.h"
#include "ast/IntLiteral.h"
#include "sema/detail/exptyper/SubscriptHandlers.h"
using namespace pycc::sema;
using namespace pycc::sema::detail;

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::Subscript &sub) {
    if (!sub.value) { addDiag(*diags, "null subscript", &sub); ok = false; return; }
    if (sub.value->kind == ast::NodeKind::SetLiteral) { addDiag(*diags, "set is not subscriptable", &sub); ok = false; return; }
    ExpressionTyper v{*env, *sigs, *retParamIdxs, *diags, polyTargets, outers}; sub.value->accept(v); if (!v.ok) { ok = false; return; }
    const uint32_t vMask = (v.outSet != 0U) ? v.outSet : TypeEnv::maskForKind(v.out);
    const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
    const uint32_t listMask = TypeEnv::maskForKind(ast::TypeKind::List);
    const uint32_t tupMask = TypeEnv::maskForKind(ast::TypeKind::Tuple);
    const uint32_t dictMask = TypeEnv::maskForKind(ast::TypeKind::Dict);
    if (vMask == strMask) { if (detail::handleSubscriptStr(sub, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, out, outSet, ok)) { if (ok) sub.setType(out); return; } }
    if (vMask == listMask) { if (detail::handleSubscriptList(sub, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, out, outSet, ok)) { if (ok) sub.setType(out); return; } }
    if (vMask == tupMask || sub.value->kind == ast::NodeKind::TupleLiteral) { if (detail::handleSubscriptTuple(sub, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, out, outSet, ok)) { if (ok) sub.setType(out); return; } }
    if (vMask == dictMask || sub.value->kind == ast::NodeKind::DictLiteral) { if (detail::handleSubscriptDict(sub, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, out, outSet, ok)) { if (ok) sub.setType(out); return; } }
    addDiag(*diags, "unsupported subscript target type", &sub); ok = false; return;
}
