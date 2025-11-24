/**
 * @file
 * @brief handleStdLibAttributeCall: Type checks known stdlib attribute calls.
 */
#include "sema/detail/exptyper/CallHandlers.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/helpers/AddDiag.h"

namespace pycc::sema::detail {
    static inline uint32_t maskOf(ast::TypeKind k, uint32_t set) { return set != 0U ? set : TypeEnv::maskForKind(k); }

    bool handleStdLibAttributeCall(const ast::Call &callNode,
                                   const TypeEnv &env,
                                   const std::unordered_map<std::string, Sig> &sigs,
                                   const std::unordered_map<std::string, int> &retParamIdxs,
                                   std::vector<Diagnostic> &diags,
                                   PolyPtrs polyTargets,
                                   const std::vector<const TypeEnv *> *outers,
                                   ast::TypeKind &out,
                                   uint32_t &outSet,
                                   bool &ok) {
        if (!callNode.callee || callNode.callee->kind != ast::NodeKind::Attribute)
            return false;
        const auto *at = static_cast<const ast::Attribute *>(callNode.callee.get());
        if (!at->value || at->value->kind != ast::NodeKind::Name)
            return false;
        const auto *base = static_cast<const ast::Name *>(at->value.get());
        const std::string fn = at->attr;

        if (base->id == "math") {
            auto checkUnary = [&](const ast::TypeKind retKind) {
                if (callNode.args.size() != 1) {
                    addDiag(diags, std::string("math.") + fn + "() takes 1 arg", &callNode);
                    ok = false;
                    return;
                }
                ExpressionTyper a{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a);
                if (!a.ok) {
                    ok = false;
                    return;
                }
                const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(
                                            ast::TypeKind::Float);
                if ((maskOf(a.out, a.outSet) & ~okmask) != 0U) {
                    addDiag(diags, std::string("math.") + fn + ": argument must be int/float", callNode.args[0].get());
                    ok = false;
                    return;
                }
                out = retKind;
                outSet = TypeEnv::maskForKind(retKind);
                const_cast<ast::Call &>(callNode).setType(out);
            };
            auto checkBinary = [&](const ast::TypeKind retKind) {
                if (callNode.args.size() != 2) {
                    addDiag(diags, std::string("math.") + fn + "() takes 2 args", &callNode);
                    ok = false;
                    return;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                if (!a0.ok) {
                    ok = false;
                    return;
                }
                ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[1]->accept(a1);
                if (!a1.ok) {
                    ok = false;
                    return;
                }
                const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(
                                            ast::TypeKind::Float);
                if ((maskOf(a0.out, a0.outSet) & ~okmask) != 0U || (maskOf(a1.out, a1.outSet) & ~okmask) != 0U) {
                    addDiag(diags, std::string("math.") + fn + ": arguments must be int/float", &callNode);
                    ok = false;
                    return;
                }
                out = retKind;
                outSet = TypeEnv::maskForKind(retKind);
                const_cast<ast::Call &>(callNode).setType(out);
            };
            if (fn == "sqrt" || fn == "fabs" || fn == "sin" || fn == "cos" || fn == "tan" || fn == "asin" || fn ==
                "acos" || fn == "atan" || fn == "exp" || fn == "exp2" || fn == "log" || fn == "log2" || fn == "log10" ||
                fn == "degrees" || fn == "radians") {
                checkUnary(ast::TypeKind::Float);
                return true;
            }
            if (fn == "floor" || fn == "ceil" || fn == "trunc") {
                checkUnary(ast::TypeKind::Int);
                return true;
            }
            if (fn == "pow" || fn == "copysign" || fn == "atan2" || fn == "fmod" || fn == "hypot") {
                checkBinary(ast::TypeKind::Float);
                return true;
            }
            return false;
        }
        if (base->id == "io") {
            if (fn == "write_stdout" || fn == "write_stderr") {
                if (callNode.args.size() != 1) { addDiag(diags, std::string("io.") + fn + "() takes 1 arg", &callNode); ok = false; return true; }
                ExpressionTyper a{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a); if (!a.ok) { ok=false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a.out, a.outSet) & ~strMask) != 0U) { addDiag(diags, std::string("io.") + fn + ": argument must be str", callNode.args[0].get()); ok = false; return true; }
                out = ast::TypeKind::NoneType; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "read_file") {
                if (callNode.args.size() != 1) { addDiag(diags, "io.read_file() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a); if (!a.ok) { ok=false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a.out, a.outSet) & ~strMask) != 0U) { addDiag(diags, "io.read_file: path must be str", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "write_file") {
                if (callNode.args.size() != 2) { addDiag(diags, "io.write_file() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper p{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(p);
                ExpressionTyper s{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(s);
                if (!p.ok || !s.ok) { ok=false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(p.out, p.outSet) & ~strMask) != 0U || (maskOf(s.out, s.outSet) & ~strMask) != 0U) {
                    addDiag(diags, "io.write_file: args must be str", &callNode); ok=false; return true; }
                out = ast::TypeKind::Bool; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base->id == "subprocess") {
            if (callNode.args.size() != 1) {
                addDiag(diags, std::string("subprocess.") + fn + "() takes 1 arg", &callNode);
                ok = false;
                return true;
            }
            ExpressionTyper a{env, sigs, retParamIdxs, diags, polyTargets, outers};
            callNode.args[0]->accept(a);
            if (!a.ok) {
                ok = false;
                return true;
            }
            const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
            if ((maskOf(a.out, a.outSet) & ~strMask) != 0U) {
                addDiag(diags, std::string("subprocess.") + fn + ": argument must be str", callNode.args[0].get());
                ok = false;
                return true;
            }
            out = ast::TypeKind::Int;
            outSet = TypeEnv::maskForKind(out);
            const_cast<ast::Call &>(callNode).setType(out);
            return true;
        }
        if (base->id == "sys") {
            if (fn == "exit") {
                if (callNode.args.size() != 1) {
                    addDiag(diags, "sys.exit() takes 1 arg", &callNode);
                    ok = false;
                    return true;
                }
                ExpressionTyper a{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a);
                if (!a.ok) {
                    ok = false;
                    return true;
                }
                const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int) |
                                       TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(
                                           ast::TypeKind::Float);
                if ((maskOf(a.out, a.outSet) & ~allow) != 0U) {
                    addDiag(diags, "sys.exit: int/bool/float required", callNode.args[0].get());
                    ok = false;
                    return true;
                }
                out = ast::TypeKind::NoneType;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            if (fn == "platform" || fn == "version") {
                if (!callNode.args.empty()) {
                    addDiag(diags, std::string("sys.") + fn + "() takes 0 args", &callNode);
                    ok = false;
                    return true;
                }
                out = ast::TypeKind::Str;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            if (fn == "maxsize") {
                if (!callNode.args.empty()) {
                    addDiag(diags, "sys.maxsize() takes 0 args", &callNode);
                    ok = false;
                    return true;
                }
                out = ast::TypeKind::Int;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            return false;
        }
        if (base->id == "fnmatch") {
            if (fn == "fnmatch" || fn == "fnmatchcase") {
                if (callNode.args.size() != 2) {
                    addDiag(diags, std::string("fnmatch.") + fn + "() takes 2 args", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[1]->accept(a1);
                if (!a0.ok || !a1.ok) { ok = false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~strMask) != 0U || (maskOf(a1.out, a1.outSet) & ~strMask) != 0U) {
                    addDiag(diags, std::string("fnmatch.") + fn + ": arguments must be str", &callNode);
                    ok = false; return true;
                }
                out = ast::TypeKind::Bool;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            if (fn == "filter") {
                if (callNode.args.size() != 2) {
                    addDiag(diags, "fnmatch.filter() takes 2 args", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[1]->accept(a1);
                if (!a0.ok || !a1.ok) { ok = false; return true; }
                const uint32_t allowList = TypeEnv::maskForKind(ast::TypeKind::List);
                const uint32_t allowStr = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~allowList) != 0U || (maskOf(a1.out, a1.outSet) & ~allowStr) != 0U) {
                    addDiag(diags, "fnmatch.filter: (list, str) required", &callNode);
                    ok = false; return true;
                }
                out = ast::TypeKind::List;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            if (fn == "translate") {
                if (callNode.args.size() != 1) {
                    addDiag(diags, "fnmatch.translate() takes 1 arg", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                if (!a0.ok) { ok = false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~strMask) != 0U) {
                    addDiag(diags, "fnmatch.translate: str required", &callNode);
                    ok = false; return true;
                }
                out = ast::TypeKind::Str;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            return false;
        }
        if (base->id == "os") {
            if (fn == "mkdir") {
                if (!(callNode.args.size() == 1 || callNode.args.size() == 2)) { addDiag(diags, "os.mkdir() takes 1 or 2 args", &callNode); ok=false; return true; }
                ExpressionTyper p{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(p); if (!p.ok) { ok=false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(p.out, p.outSet) & ~strMask) != 0U) { addDiag(diags, "os.mkdir: path must be str", callNode.args[0].get()); ok=false; return true; }
                if (callNode.args.size() == 2) { ExpressionTyper m{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(m); if (!m.ok) { ok=false; return true; } const uint32_t intMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool); if ((maskOf(m.out, m.outSet) & ~intMask) != 0U) { addDiag(diags, "os.mkdir: mode must be int/bool", callNode.args[1].get()); ok=false; return true; } }
                out = ast::TypeKind::Bool; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "remove" || fn == "rename") {
                const size_t need = (fn == "remove" ? 1u : 2u);
                if (callNode.args.size() != need) { addDiag(diags, std::string("os.") + fn + (fn=="remove"?"() takes 1 arg":"() takes 2 args"), &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0); if (!a0.ok) { ok=false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~strMask) != 0U) { addDiag(diags, std::string("os.") + fn + ": path must be str", callNode.args[0].get()); ok=false; return true; }
                if (fn == "rename") { ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(a1); if (!a1.ok) { ok=false; return true; } if ((maskOf(a1.out, a1.outSet) & ~strMask) != 0U) { addDiag(diags, "os.rename: dest must be str", callNode.args[1].get()); ok=false; return true; } }
                out = ast::TypeKind::Bool; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base->id == "binascii") {
            if (fn == "hexlify") {
                if (callNode.args.size() != 1) { addDiag(diags, "binascii.hexlify() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                if (!a0.ok) { ok=false; return true; }
                const uint32_t byMask = TypeEnv::maskForKind(ast::TypeKind::Bytes);
                if ((maskOf(a0.out, a0.outSet) & ~byMask) != 0U && callNode.args[0]->kind != ast::NodeKind::BytesLiteral) {
                    addDiag(diags, "binascii.hexlify: argument must be bytes", callNode.args[0].get()); ok=false; return true;
                }
                out = ast::TypeKind::Bytes; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "unhexlify") {
                if (callNode.args.size() != 1) { addDiag(diags, "binascii.unhexlify() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0); if (!a0.ok) { ok=false; return true; }
                const uint32_t byOrStr = TypeEnv::maskForKind(ast::TypeKind::Bytes) | TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~byOrStr) != 0U) { addDiag(diags, "binascii.unhexlify: argument must be str or bytes", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Bytes; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base->id == "glob") {
            if (fn == "glob" || fn == "iglob") {
                if (callNode.args.size() != 1) {
                    addDiag(diags, std::string("glob.") + fn + "() takes 1 arg", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                if (!a0.ok) { ok = false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~strMask) != 0U) {
                    addDiag(diags, std::string("glob.") + fn + ": argument must be str", &callNode);
                    ok = false; return true;
                }
                out = ast::TypeKind::List;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            if (fn == "escape") {
                if (callNode.args.size() != 1) {
                    addDiag(diags, "glob.escape() takes 1 arg", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                if (!a0.ok) { ok = false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~strMask) != 0U) {
                    addDiag(diags, "glob.escape: str required", &callNode);
                    ok = false; return true;
                }
                out = ast::TypeKind::Str;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            return false;
        }
        // Generic attribute shims: string/bytes encode/decode
        if (fn == "decode") {
            // Base must be bytes; args are 0..2 strs; returns str
            if (callNode.args.size() > 2) { addDiag(diags, "decode() takes 0, 1, or 2 args", &callNode); ok=false; return true; }
            // Check base type
            if (callNode.callee && callNode.callee->kind == ast::NodeKind::Attribute) {
                const auto *aat = static_cast<const ast::Attribute *>(callNode.callee.get());
                ExpressionTyper baseTy{env, sigs, retParamIdxs, diags, polyTargets, outers}; aat->value->accept(baseTy);
                if (!baseTy.ok) { ok=false; return true; }
                const uint32_t byMask = TypeEnv::maskForKind(ast::TypeKind::Bytes);
                if ((maskOf(baseTy.out, baseTy.outSet) & ~byMask) != 0U) { addDiag(diags, "decode(): base must be bytes", aat->value.get()); ok=false; return true; }
            }
            for (const auto &arg : callNode.args) {
                if (!arg) continue; ExpressionTyper at0{env, sigs, retParamIdxs, diags, polyTargets, outers}; arg->accept(at0); if (!at0.ok) { ok=false; return true; }
                const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(at0.out, at0.outSet) & ~allow) != 0U) { addDiag(diags, "decode(): arguments must be str", arg.get()); ok=false; return true; }
            }
            out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
        }
        if (fn == "encode") {
            // Base must be str; args 0..2 strs; returns bytes
            if (callNode.args.size() > 2) { addDiag(diags, "encode() takes 0, 1, or 2 args", &callNode); ok=false; return true; }
            if (callNode.callee && callNode.callee->kind == ast::NodeKind::Attribute) {
                const auto *aat = static_cast<const ast::Attribute *>(callNode.callee.get());
                ExpressionTyper baseTy{env, sigs, retParamIdxs, diags, polyTargets, outers}; aat->value->accept(baseTy);
                if (!baseTy.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(baseTy.out, baseTy.outSet) & ~sMask) != 0U) { addDiag(diags, "encode(): base must be str", aat->value.get()); ok=false; return true; }
            }
            for (const auto &arg : callNode.args) {
                if (!arg) continue; ExpressionTyper at0{env, sigs, retParamIdxs, diags, polyTargets, outers}; arg->accept(at0); if (!at0.ok) { ok=false; return true; }
                const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(at0.out, at0.outSet) & ~allow) != 0U) { addDiag(diags, "encode(): arguments must be str", arg.get()); ok=false; return true; }
            }
            out = ast::TypeKind::Bytes; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
        }
        // Minimal typing shims for json module
        if (base->id == "json") {
            if (fn == "dumps") {
                if (!(callNode.args.size() == 1 || callNode.args.size() == 2)) { addDiag(diags, "json.dumps() takes 1 or 2 args", &callNode); ok=false; return true; }
                if (callNode.args.size() == 2) {
                    ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(a1); if (!a1.ok) { ok=false; return true; }
                    const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool);
                    if ((maskOf(a1.out, a1.outSet) & ~allow) != 0U) { addDiag(diags, "json.dumps: indent must be int/bool", callNode.args[1].get()); ok=false; return true; }
                }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "loads") {
                if (callNode.args.size() != 1) { addDiag(diags, "json.loads() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0); if (!a0.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~sMask) != 0U) { addDiag(diags, "json.loads: argument must be str", callNode.args[0].get()); ok=false; return true; }
                // Return dynamic object; in this subset we'll treat as Str for downstream dumps compatibility
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        // Minimal typing for re module
        if (base->id == "re") {
            if (fn == "search" || fn == "match" || fn == "fullmatch") {
                if (!(callNode.args.size() == 2 || callNode.args.size() == 3)) { addDiag(diags, std::string("re.") + fn + "() takes 2 or 3 args", &callNode); ok=false; return true; }
                ExpressionTyper p{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(p); if (!p.ok) { ok=false; return true; }
                ExpressionTyper t{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(t); if (!t.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(p.out, p.outSet) & ~sMask) != 0U || (maskOf(t.out, t.outSet) & ~sMask) != 0U) { addDiag(diags, std::string("re.") + fn + ": pattern/text must be str", &callNode); ok=false; return true; }
                if (callNode.args.size() == 3) { ExpressionTyper f{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[2]->accept(f); if (!f.ok) { ok=false; return true; } const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool); if ((maskOf(f.out, f.outSet) & ~iMask) != 0U) { addDiag(diags, "re flags must be int/bool", callNode.args[2].get()); ok=false; return true; } }
                // Return opaque match object pointer, modeled as Str for pointer-compat use
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "sub") {
                if (!(callNode.args.size() == 3 || callNode.args.size() == 4)) { addDiag(diags, "re.sub() takes 3 or 4 args", &callNode); ok=false; return true; }
                ExpressionTyper p{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(p);
                ExpressionTyper r{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(r);
                ExpressionTyper s{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[2]->accept(s);
                if (!p.ok || !r.ok || !s.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(p.out, p.outSet) & ~sMask) != 0U || (maskOf(r.out, r.outSet) & ~sMask) != 0U || (maskOf(s.out, s.outSet) & ~sMask) != 0U) { addDiag(diags, "re.sub: pattern/repl/text must be str", &callNode); ok=false; return true; }
                if (callNode.args.size() == 4) { ExpressionTyper c{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[3]->accept(c); if (!c.ok) { ok=false; return true; } const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool); if ((maskOf(c.out, c.outSet) & ~iMask) != 0U) { addDiag(diags, "re.sub: count must be int/bool", callNode.args[3].get()); ok=false; return true; } }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        // Minimal typing for itertools: we treat outputs as lists
        if (base->id == "itertools") {
            if (fn == "permutations" || fn == "combinations" || fn == "combinations_with_replacement") {
                if (!(callNode.args.size() == 1 || callNode.args.size() == 2) && fn == "permutations") { addDiag(diags, "itertools.permutations() takes 1 or 2 args", &callNode); ok=false; return true; }
                if (fn != "permutations" && callNode.args.size() != 2) { addDiag(diags, std::string("itertools.") + fn + "() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper a{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a); if (!a.ok) { ok=false; return true; }
                const uint32_t lMask = TypeEnv::maskForKind(ast::TypeKind::List);
                if ((maskOf(a.out, a.outSet) & ~lMask) != 0U) { addDiag(diags, std::string("itertools.") + fn + ": first arg must be list", callNode.args[0].get()); ok=false; return true; }
                if ((fn == "permutations" && callNode.args.size() == 2) || fn != "permutations") {
                    ExpressionTyper r{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(r); if (!r.ok) { ok=false; return true; }
                    const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool);
                    if ((maskOf(r.out, r.outSet) & ~iMask) != 0U) { addDiag(diags, std::string("itertools.") + fn + ": r must be int/bool", callNode.args[1].get()); ok=false; return true; }
                }
                out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        // Minimal typing for pathlib: paths are strings; booleans for predicates
        if (base->id == "pathlib") {
            if (fn == "cwd" || fn == "home") { if (!callNode.args.empty()) { addDiag(diags, std::string("pathlib.") + fn + "() takes 0 args", &callNode); ok=false; return true; } out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            if (fn == "join") { if (callNode.args.size()!=2) { addDiag(diags, "pathlib.join() takes 2 args", &callNode); ok=false; return true; } ExpressionTyper a{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(a); if (!a.ok) { ok=false; return true; } ExpressionTyper b{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[1]->accept(b); if (!b.ok) { ok=false; return true; } const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str); if ((maskOf(a.out,a.outSet)&~sMask)!=0U || (maskOf(b.out,b.outSet)&~sMask)!=0U) { addDiag(diags, "pathlib.join: arguments must be str", &callNode); ok=false; return true; } out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            if (fn == "parent" || fn == "basename" || fn == "suffix" || fn == "stem" || fn == "as_posix" || fn == "as_uri" || fn == "resolve" || fn == "absolute") { if (callNode.args.size()!=1) { addDiag(diags, std::string("pathlib.")+fn+"() takes 1 arg", &callNode); ok=false; return true; } ExpressionTyper p{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(p); if (!p.ok) { ok=false; return true; } const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str); if ((maskOf(p.out,p.outSet)&~sMask)!=0U) { addDiag(diags, std::string("pathlib.")+fn+": path must be str", callNode.args[0].get()); ok=false; return true; } out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            if (fn == "exists" || fn == "is_file" || fn == "is_dir" || fn == "match") { const size_t need = (fn=="match"?2u:1u); if (callNode.args.size()!=need) { addDiag(diags, std::string("pathlib.")+fn+ (need==1?"() takes 1 arg":"() takes 2 args"), &callNode); ok=false; return true; } ExpressionTyper p{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(p); if (!p.ok) { ok=false; return true; } const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str); if ((maskOf(p.out,p.outSet)&~sMask)!=0U) { addDiag(diags, std::string("pathlib.")+fn+": path must be str", callNode.args[0].get()); ok=false; return true; } if (need==2) { ExpressionTyper q{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[1]->accept(q); if (!q.ok) { ok=false; return true; } if ((maskOf(q.out,q.outSet)&~sMask)!=0U) { addDiag(diags, "pathlib.match: pattern must be str", callNode.args[1].get()); ok=false; return true; } } out = ast::TypeKind::Bool; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            return false;
        }
        return false;
    }
} // namespace pycc::sema::detail
