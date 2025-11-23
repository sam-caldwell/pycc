/***
 * Name: ExpressionTyper::visit(Call)
 * Purpose: Thin dispatcher that delegates call typing to helpers.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/detail/Helpers.h"
#include "sema/detail/exptyper/CallHandlers.h"
#include "sema/detail/exptyper/CallResolve.h"
#include "sema/detail/exptyper/CallBuiltins.h"
#include "ast/Call.h"
#include "ast/Attribute.h"
#include "ast/Name.h"

using namespace pycc;
using namespace pycc::sema;

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ExpressionTyper::visit(const ast::Call &callNode) {
    if (detail::handleStdLibAttributeCall(callNode, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, out, outSet, ok)) return;
    if (detail::handleBuiltinCall(callNode, *env, *sigs, *retParamIdxs, *diags, polyTargets, out, outSet, ok)) return;

    if (callNode.callee && callNode.callee->kind == ast::NodeKind::Name) {
        const auto *nameNode = static_cast<const ast::Name *>(callNode.callee.get());
        const bool handled = detail::resolveNamedCall(callNode, nameNode, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes, out, outSet, ok);
        if (handled) {
            if (ok) {
                auto it = retParamIdxs->find(nameNode->id);
                if (it != retParamIdxs->end()) {
                    const int idx = it->second;
                    if (idx >= 0 && static_cast<size_t>(idx) < callNode.args.size()) {
                        const auto &arg = callNode.args[idx];
                        if (arg) {
                            const auto &can = arg->canonical();
                            if (can) { callNode.setCanonicalKey(*can); }
                        }
                    }
                }
            }
            return;
        }
    }
    if (callNode.callee && callNode.callee->kind == ast::NodeKind::Attribute) {
        const auto *at = static_cast<const ast::Attribute *>(callNode.callee.get());
        if (detail::resolveAttributeCall(callNode, at, *env, *sigs, *retParamIdxs, *diags, polyTargets, outers, classes, out, outSet, ok)) return;
    }
    addDiag(*diags, "unknown call target", &callNode);
    ok = false;
}

