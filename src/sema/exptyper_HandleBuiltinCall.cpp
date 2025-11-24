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
            std::string msg = std::string("len() argument must be str/list/tuple/dict (got ") + pycc::ast::to_string(k) + ")";
            addDiag(diags, msg, callNode.args[0].get());
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
    // Concurrency builtins: chan_new/chan_send/chan_recv
    if (nameNode->id == "chan_new") {
        if (callNode.args.size() != 1) {
            addDiag(diags, "chan_new() takes exactly 1 argument", &callNode);
            ok = false; return true;
        }
        ExpressionTyper capTyper{env, sigs, /*retParamIdxs*/{}, diags, polyTargets};
        callNode.args[0]->accept(capTyper);
        if (!capTyper.ok) { ok = false; return true; }
        if (!(capTyper.out == ast::TypeKind::Int || capTyper.out == ast::TypeKind::Bool)) {
            std::string msg = std::string("chan_new(cap): capacity must be int or bool (got ") + pycc::ast::to_string(capTyper.out) + ")";
            addDiag(diags, msg, callNode.args[0].get());
            ok = false; return true;
        }
        out = ast::TypeKind::NoneType; callNode.setType(out); return true;
    }
    if (nameNode->id == "chan_send") {
        if (callNode.args.size() != 2) {
            addDiag(diags, "chan_send() takes exactly 2 arguments", &callNode);
            ok = false; return true;
        }
        // Only enforce payload immutability at type-time: allowed int/float/bool/str/bytes literal.
        ExpressionTyper payloadTyper{env, sigs, /*retParamIdxs*/{}, diags, polyTargets};
        callNode.args[1]->accept(payloadTyper);
        if (!payloadTyper.ok) { ok = false; return true; }
        const bool isBytesLiteral = (callNode.args[1] && callNode.args[1]->kind == ast::NodeKind::BytesLiteral);
        const auto t = payloadTyper.out;
        const bool allowed = (t == ast::TypeKind::Int || t == ast::TypeKind::Float || t == ast::TypeKind::Bool || t == ast::TypeKind::Str || isBytesLiteral);
        const bool disallowedContainer = (t == ast::TypeKind::List || t == ast::TypeKind::Tuple || t == ast::TypeKind::Dict) || (callNode.args[1] && callNode.args[1]->kind == ast::NodeKind::ObjectLiteral);
        if (!allowed || disallowedContainer) {
            addDiag(diags, "chan_send: payload must be immutable (int/float/bool/str/bytes)", callNode.args[1].get());
            ok = false; return true;
        }
        out = ast::TypeKind::NoneType; callNode.setType(out); return true;
    }
    if (nameNode->id == "chan_recv") {
        if (callNode.args.size() != 1) {
            addDiag(diags, "chan_recv() takes exactly 1 argument", &callNode);
            ok = false; return true;
        }
        // Unknown dynamic type at compile-time; treat as opaque
        out = ast::TypeKind::NoneType; callNode.setType(out); return true;
    }
    return false;
}

} // namespace pycc::sema::detail
