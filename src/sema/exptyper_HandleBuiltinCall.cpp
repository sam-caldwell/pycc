/**
 * @file
 * @brief handleBuiltinCall: Handle builtin name calls (len, eval/exec, obj_get).
 */
#include "sema/detail/exptyper/CallBuiltins.h"
#include "sema/detail/ExpressionTyper.h"

namespace pycc::sema::detail {

bool handleBuiltinCall(const ast::Call& callNode,
                       const TypeEnv& env,
                       const std::unordered_map<std::string, Sig>& sigs,
                       const std::unordered_map<std::string, int>& /*retParamIdxs*/,
                       std::vector<Diagnostic>& diags,
                       PolyPtrs polyTargets,
                       ast::TypeKind& out,
                       uint32_t& outSet,
                       bool& ok) {
    (void)outSet;
    if (!(callNode.callee && callNode.callee->kind == ast::NodeKind::Name)) return false;
    const auto* nameNode = static_cast<const ast::Name*>(callNode.callee.get());

    // eval/exec on literal strings only in this subset
    if (nameNode->id == "eval" || nameNode->id == "exec") {
        if (callNode.args.size() != 1 || !callNode.args[0] || callNode.args[0]->kind != ast::NodeKind::StringLiteral) {
            addDiag(diags, std::string(nameNode->id) + "() only accepts a compile-time literal string in this subset",
                   &callNode);
            ok = false;
            return true;
        }
        out = ast::TypeKind::NoneType;
        callNode.setType(out);
        return true;
    }

    // len(x) -> int for str/list/tuple/dict
    if (nameNode->id == "len") {
        if (callNode.args.size() != 1) {
            addDiag(diags, "len() takes exactly one argument", &callNode);
            ok = false;
            return true;
        }
        ExpressionTyper argTyper{env, sigs, /*retParamIdxs*/{}, diags, polyTargets};
        callNode.args[0]->accept(argTyper);
        if (!argTyper.ok) { ok = false; return true; }
        const ast::TypeKind k = argTyper.out;
        if (!(k == ast::TypeKind::Str || k == ast::TypeKind::List || k == ast::TypeKind::Tuple || k == ast::TypeKind::Dict)) {
            addDiag(diags, "len() argument must be str/list/tuple/dict", callNode.args[0].get());
            ok = false;
            return true;
        }
        out = ast::TypeKind::Int;
        callNode.setType(out);
        return true;
    }

    // obj_get(o, i) -> str (opaque object field access by index). Only enforce index is int.
    if (nameNode->id == "obj_get") {
        if (callNode.args.size() != 2) {
            addDiag(diags, "obj_get() takes two arguments", &callNode);
            ok = false;
            return true;
        }
        ExpressionTyper idxTyper{env, sigs, /*retParamIdxs*/{}, diags, polyTargets};
        if (callNode.args[1]) callNode.args[1]->accept(idxTyper);
        if (!idxTyper.ok) { ok = false; return true; }
        if (!typeIsInt(idxTyper.out)) {
            addDiag(diags, "obj_get index must be int", callNode.args[1].get());
            ok = false;
            return true;
        }
        out = ast::TypeKind::Str;
        callNode.setType(out);
        return true;
    }

    (void)sigs;
    return false;
}

} // namespace pycc::sema::detail

