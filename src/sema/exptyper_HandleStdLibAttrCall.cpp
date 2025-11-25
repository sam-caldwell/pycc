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
        const std::string fn = at->attr;
        // Base may be any expression (for generic attribute methods like encode/decode);
        // capture name when available for module-dispatch below.
        const ast::Name *base = nullptr;
        if (at->value && at->value->kind == ast::NodeKind::Name) {
            base = static_cast<const ast::Name *>(at->value.get());
        }

        if (base && base->id == "math") {
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
        if (base && base->id == "io") {
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
        if (base && base->id == "fnmatch") {
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
        if (base && base->id == "os") {
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
        if (base && base->id == "binascii") {
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
        if (base && base->id == "shutil") {
            // shutil.copyfile(src: str, dst: str) -> bool; shutil.copy(src: str, dst: str) -> bool
            if (fn == "copyfile" || fn == "copy") {
                if (callNode.args.size() != 2) {
                    addDiag(diags, std::string("shutil.") + fn + "() takes 2 args", &callNode);
                    ok = false; return true;
                }
                // src
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                if (!a0.ok) { ok = false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~sMask) != 0U) {
                    addDiag(diags, std::string("shutil.") + fn + ": src must be str", callNode.args[0].get());
                    ok = false; return true;
                }
                // dst
                ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[1]->accept(a1);
                if (!a1.ok) { ok = false; return true; }
                if ((maskOf(a1.out, a1.outSet) & ~sMask) != 0U) {
                    addDiag(diags, std::string("shutil.") + fn + ": dst must be str", callNode.args[1].get());
                    ok = false; return true;
                }
                out = ast::TypeKind::Bool;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            return false;
        }
        if (base && base->id == "glob") {
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
        if (base && base->id == "base64") {
            // base64.b64encode(x: str|bytes) -> bytes; base64.b64decode(x: str|bytes) -> bytes
            if (fn == "b64encode" || fn == "b64decode") {
                if (callNode.args.size() != 1) {
                    addDiag(diags, std::string("base64.") + fn + "() takes 1 arg", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                if (!a0.ok) { ok = false; return true; }
                const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Str) | TypeEnv::maskForKind(ast::TypeKind::Bytes);
                if ((maskOf(a0.out, a0.outSet) & ~allow) != 0U) {
                    addDiag(diags, std::string("base64.") + fn + ": argument must be str or bytes", callNode.args[0].get());
                    ok = false; return true;
                }
                out = ast::TypeKind::Bytes;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            return false;
        }
        if (base && base->id == "hashlib") {
            // hashlib.sha256(x: str|bytes) -> str (hex); hashlib.md5(x: str|bytes) -> str
            if (fn == "sha256" || fn == "md5") {
                if (callNode.args.size() != 1) {
                    addDiag(diags, std::string("hashlib.") + fn + "() takes 1 arg", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(a0);
                if (!a0.ok) { ok = false; return true; }
                const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Str) | TypeEnv::maskForKind(ast::TypeKind::Bytes);
                if ((maskOf(a0.out, a0.outSet) & ~allow) != 0U) {
                    addDiag(diags, std::string("hashlib.") + fn + ": argument must be str or bytes", callNode.args[0].get());
                    ok = false; return true;
                }
                out = ast::TypeKind::Str;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            return false;
        }
        if (base && base->id == "hmac") {
            // hmac.digest(key: str|bytes, msg: str|bytes, digestmod: str) -> Bytes
            if (fn == "digest") {
                if (callNode.args.size() != 3) {
                    addDiag(diags, "hmac.digest() takes 3 args", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper k{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(k);
                ExpressionTyper m{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(m);
                ExpressionTyper a{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[2]->accept(a);
                if (!k.ok || !m.ok || !a.ok) { ok = false; return true; }
                const uint32_t byOrStr = TypeEnv::maskForKind(ast::TypeKind::Str) | TypeEnv::maskForKind(ast::TypeKind::Bytes);
                if ((maskOf(k.out, k.outSet) & ~byOrStr) != 0U) { addDiag(diags, "hmac.digest: key must be str|bytes", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(m.out, m.outSet) & ~byOrStr) != 0U) { addDiag(diags, "hmac.digest: msg must be str|bytes", callNode.args[1].get()); ok=false; return true; }
                const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a.out, a.outSet) & ~strMask) != 0U) { addDiag(diags, "hmac.digest: digestmod must be str", callNode.args[2].get()); ok=false; return true; }
                out = ast::TypeKind::Bytes; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "html") {
            // html.escape(s: str, quote: bool|int=1) -> str; html.unescape(s: str) -> str
            if (fn == "escape") {
                if (!(callNode.args.size() == 1 || callNode.args.size() == 2)) {
                    addDiag(diags, "html.escape() takes 1 or 2 args", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper s{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(s);
                if (!s.ok) { ok = false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(s.out, s.outSet) & ~sMask) != 0U) { addDiag(diags, "html.escape: argument must be str", callNode.args[0].get()); ok=false; return true; }
                if (callNode.args.size() == 2) {
                    ExpressionTyper q{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(q);
                    if (!q.ok) { ok=false; return true; }
                    const uint32_t qb = TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
                    if ((maskOf(q.out, q.outSet) & ~qb) != 0U) { addDiag(diags, "html.escape: quote must be bool/numeric", callNode.args[1].get()); ok=false; return true; }
                }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "unescape") {
                if (callNode.args.size() != 1) { addDiag(diags, "html.unescape() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper s{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(s);
                if (!s.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(s.out, s.outSet) & ~sMask) != 0U) { addDiag(diags, "html.unescape: argument must be str", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "unicodedata") {
            // unicodedata.normalize(form: str, s: str) -> str
            if (fn == "normalize") {
                if (callNode.args.size() != 2) {
                    addDiag(diags, "unicodedata.normalize() takes 2 args", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                if (!a0.ok) { ok=false; return true; }
                ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(a1);
                if (!a1.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~sMask) != 0U) { addDiag(diags, "unicodedata.normalize: form must be str", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(a1.out, a1.outSet) & ~sMask) != 0U) { addDiag(diags, "unicodedata.normalize: argument must be str", callNode.args[1].get()); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "getpass") {
            if (fn == "getuser") {
                if (!callNode.args.empty()) { addDiag(diags, "getpass.getuser() takes 0 args", &callNode); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "getpass") {
                if (callNode.args.size() > 1) { addDiag(diags, "getpass.getpass() takes 0 or 1 arg", &callNode); ok=false; return true; }
                if (callNode.args.size() == 1) {
                    ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                    if (!a0.ok) { ok=false; return true; }
                    const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                    if ((maskOf(a0.out, a0.outSet) & ~sMask) != 0U) { addDiag(diags, "getpass.getpass: prompt must be str", callNode.args[0].get()); ok=false; return true; }
                }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "shlex") {
            if (fn == "split") {
                if (callNode.args.size() != 1) { addDiag(diags, "shlex.split() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                if (!a0.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~sMask) != 0U) { addDiag(diags, "shlex.split: argument must be str", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "join") {
                if (callNode.args.size() != 1) { addDiag(diags, "shlex.join() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                if (!a0.ok) { ok=false; return true; }
                const uint32_t lMask = TypeEnv::maskForKind(ast::TypeKind::List);
                if ((maskOf(a0.out, a0.outSet) & ~lMask) != 0U) { addDiag(diags, "shlex.join: argument must be list", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "textwrap") {
            const auto handle_width_2args = [&](const std::string &qualname, ast::TypeKind retKind) {
                if (callNode.args.size() != 2) { addDiag(diags, std::string(qualname) + "() takes 2 args", &callNode); ok=false; return; }
                ExpressionTyper s{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(s);
                ExpressionTyper w{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(w);
                if (!s.ok || !w.ok) { ok=false; return; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                const uint32_t nMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(s.out, s.outSet) & ~sMask) != 0U) { addDiag(diags, qualname + ": first arg must be str", callNode.args[0].get()); ok=false; return; }
                if ((maskOf(w.out, w.outSet) & ~nMask) != 0U) { addDiag(diags, qualname + ": width must be numeric", callNode.args[1].get()); ok=false; return; }
                out = retKind; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out);
            };
            if (fn == "fill" || fn == "shorten") {
                handle_width_2args(std::string("textwrap.")+fn, ast::TypeKind::Str);
                return true;
            }
            if (fn == "wrap") {
                handle_width_2args(std::string("textwrap.wrap"), ast::TypeKind::List);
                return true;
            }
            if (fn == "dedent") {
                if (callNode.args.size() != 1) { addDiag(diags, "textwrap.dedent() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper s{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(s);
                if (!s.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(s.out, s.outSet) & ~sMask) != 0U) { addDiag(diags, "textwrap.dedent: argument must be str", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out); return true;
            }
            if (fn == "indent") {
                if (callNode.args.size() != 2) { addDiag(diags, "textwrap.indent() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper s{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(s);
                ExpressionTyper p{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(p);
                if (!s.ok || !p.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(s.out, s.outSet) & ~sMask) != 0U) { addDiag(diags, "textwrap.indent: first arg must be str", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(p.out, p.outSet) & ~sMask) != 0U) { addDiag(diags, "textwrap.indent: prefix must be str", callNode.args[1].get()); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "keyword") {
            if (fn == "iskeyword") {
                if (callNode.args.size() != 1) { addDiag(diags, "keyword.iskeyword() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                if (!a0.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(a0.out, a0.outSet) & ~sMask) != 0U) { addDiag(diags, "keyword.iskeyword: argument must be str", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Bool; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "kwlist") {
                if (!callNode.args.empty()) { addDiag(diags, "keyword.kwlist() takes 0 args", &callNode); ok=false; return true; }
                out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "statistics") {
            if (fn == "mean" || fn == "median" || fn == "stdev" || fn == "pvariance") {
                if (callNode.args.size() != 1) { addDiag(diags, std::string("statistics.") + fn + "() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                if (!a0.ok) { ok=false; return true; }
                const uint32_t lMask = TypeEnv::maskForKind(ast::TypeKind::List);
                if ((maskOf(a0.out, a0.outSet) & ~lMask) != 0U) { addDiag(diags, std::string("statistics.") + fn + ": argument must be list", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Float; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "tempfile") {
            if (fn == "gettempdir" || fn == "mkdtemp") {
                if (!callNode.args.empty()) { addDiag(diags, std::string("tempfile.") + fn + "() takes 0 args", &callNode); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "mkstemp") {
                if (!callNode.args.empty()) { addDiag(diags, "tempfile.mkstemp() takes 0 args", &callNode); ok=false; return true; }
                out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "bisect") {
            if (fn == "bisect_left" || fn == "bisect_right" || fn == "bisect") {
                if (callNode.args.size() != 2) { addDiag(diags, std::string("bisect.") + fn + "() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(a1);
                if (!a0.ok || !a1.ok) { ok = false; return true; }
                const uint32_t lMask = TypeEnv::maskForKind(ast::TypeKind::List);
                const uint32_t nMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(a0.out, a0.outSet) & ~lMask) != 0U) { addDiag(diags, std::string("bisect.") + fn + ": first arg must be list", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(a1.out, a1.outSet) & ~nMask) != 0U) { addDiag(diags, std::string("bisect.") + fn + ": x must be numeric", callNode.args[1].get()); ok=false; return true; }
                out = ast::TypeKind::Int; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out); return true;
            }
            if (fn == "insort_left" || fn == "insort_right" || fn == "insort") {
                if (callNode.args.size() != 2) { addDiag(diags, std::string("bisect.") + fn + "() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper a0{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(a0);
                ExpressionTyper a1{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(a1);
                if (!a0.ok || !a1.ok) { ok = false; return true; }
                const uint32_t lMask = TypeEnv::maskForKind(ast::TypeKind::List);
                const uint32_t nMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(a0.out, a0.outSet) & ~lMask) != 0U) { addDiag(diags, std::string("bisect.") + fn + ": first arg must be list", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(a1.out, a1.outSet) & ~nMask) != 0U) { addDiag(diags, std::string("bisect.") + fn + ": x must be numeric", callNode.args[1].get()); ok=false; return true; }
                out = ast::TypeKind::NoneType; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "stat") {
            if (fn == "S_IFMT") {
                if (callNode.args.size() != 1) { addDiag(diags, "stat.S_IFMT() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper m{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(m);
                if (!m.ok) { ok=false; return true; }
                const uint32_t num = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(m.out, m.outSet) & ~num) != 0U) { addDiag(diags, "stat.S_IFMT: mode must be numeric", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Int; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "S_ISDIR" || fn == "S_ISREG") {
                if (callNode.args.size() != 1) { addDiag(diags, std::string("stat.")+fn+"() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper m{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(m);
                if (!m.ok) { ok=false; return true; }
                const uint32_t num = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(m.out, m.outSet) & ~num) != 0U) { addDiag(diags, std::string("stat.")+fn+": mode must be numeric", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Bool; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "struct") {
            if (fn == "pack") {
                if (callNode.args.size() != 2) { addDiag(diags, "struct.pack() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper f{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(f);
                ExpressionTyper v{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(v);
                if (!f.ok || !v.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                const uint32_t lMask = TypeEnv::maskForKind(ast::TypeKind::List);
                if ((maskOf(f.out, f.outSet) & ~sMask) != 0U) { addDiag(diags, "struct.pack: fmt must be str", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(v.out, v.outSet) & ~lMask) != 0U) { addDiag(diags, "struct.pack: values must be list", callNode.args[1].get()); ok=false; return true; }
                out = ast::TypeKind::Bytes; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out); return true;
            }
            if (fn == "unpack") {
                if (callNode.args.size() != 2) { addDiag(diags, "struct.unpack() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper f{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(f);
                ExpressionTyper d{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(d);
                if (!f.ok || !d.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                const uint32_t bMask = TypeEnv::maskForKind(ast::TypeKind::Bytes);
                if ((maskOf(f.out, f.outSet) & ~sMask) != 0U) { addDiag(diags, "struct.unpack: fmt must be str", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(d.out, d.outSet) & ~bMask) != 0U) { addDiag(diags, "struct.unpack: data must be bytes", callNode.args[1].get()); ok=false; return true; }
                out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out); return true;
            }
            if (fn == "calcsize") {
                if (callNode.args.size() != 1) { addDiag(diags, "struct.calcsize() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper f{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(f);
                if (!f.ok) { ok=false; return true; }
                const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str);
                if ((maskOf(f.out, f.outSet) & ~sMask) != 0U) { addDiag(diags, "struct.calcsize: fmt must be str", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::Int; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call &>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "calendar") {
            if (fn == "isleap") {
                if (callNode.args.size() != 1) { addDiag(diags, "calendar.isleap() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper y{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(y);
                if (!y.ok) { ok=false; return true; }
                const uint32_t num = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(y.out, y.outSet) & ~num) != 0U) { addDiag(diags, "calendar.isleap: year must be numeric", callNode.args[0].get()); ok=false; return true; }
                // Runtime returns int (0/1), but bool works for downstream checks
                out = ast::TypeKind::Int; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "monthrange") {
                if (callNode.args.size() != 2) { addDiag(diags, "calendar.monthrange() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper y{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[0]->accept(y);
                ExpressionTyper m{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[1]->accept(m);
                if (!y.ok || !m.ok) { ok=false; return true; }
                const uint32_t num = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(y.out, y.outSet) & ~num) != 0U) { addDiag(diags, "calendar.monthrange: year must be numeric", callNode.args[0].get()); ok=false; return true; }
                if ((maskOf(m.out, m.outSet) & ~num) != 0U) { addDiag(diags, "calendar.monthrange: month must be numeric", callNode.args[1].get()); ok=false; return true; }
                out = ast::TypeKind::List; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "secrets") {
            // secrets.token_bytes(n: int) -> Bytes; token_hex(n: int) -> Str; token_urlsafe(n: int) -> Str
            if (fn == "token_bytes" || fn == "token_hex" || fn == "token_urlsafe") {
                if (callNode.args.size() != 1) {
                    addDiag(diags, std::string("secrets.") + fn + "() takes 1 arg", &callNode);
                    ok = false; return true;
                }
                ExpressionTyper narg{env, sigs, retParamIdxs, diags, polyTargets, outers};
                callNode.args[0]->accept(narg);
                if (!narg.ok) { ok = false; return true; }
                const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool);
                if ((maskOf(narg.out, narg.outSet) & ~iMask) != 0U) {
                    addDiag(diags, std::string("secrets.") + fn + ": n must be int/bool", callNode.args[0].get());
                    ok = false; return true;
                }
                const bool isBytes = (fn == "token_bytes");
                out = isBytes ? ast::TypeKind::Bytes : ast::TypeKind::Str;
                outSet = TypeEnv::maskForKind(out);
                const_cast<ast::Call &>(callNode).setType(out);
                return true;
            }
            return false;
        }
        if (base && base->id == "random") {
            if (fn == "random") {
                if (!callNode.args.empty()) { addDiag(diags, "random.random() takes 0 args", &callNode); ok=false; return true; }
                out = ast::TypeKind::Float; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "randint") {
                if (callNode.args.size() != 2) { addDiag(diags, "random.randint() takes 2 args", &callNode); ok=false; return true; }
                ExpressionTyper a0{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(a0);
                ExpressionTyper a1{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[1]->accept(a1);
                if (!a0.ok || !a1.ok) { ok=false; return true; }
                const uint32_t num = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(a0.out,a0.outSet)&~num)!=0U || (maskOf(a1.out,a1.outSet)&~num)!=0U) { addDiag(diags, "random.randint: numeric args required", &callNode); ok=false; return true; }
                out = ast::TypeKind::Int; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            if (fn == "seed") {
                if (callNode.args.size() != 1) { addDiag(diags, "random.seed() takes 1 arg", &callNode); ok=false; return true; }
                ExpressionTyper a0{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(a0);
                if (!a0.ok) { ok=false; return true; }
                const uint32_t num = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
                if ((maskOf(a0.out,a0.outSet)&~num)!=0U) { addDiag(diags, "random.seed: numeric required", callNode.args[0].get()); ok=false; return true; }
                out = ast::TypeKind::NoneType; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
            }
            return false;
        }
        if (base && base->id == "uuid") {
            if (fn == "uuid4") {
                if (!callNode.args.empty()) { addDiag(diags, "uuid.uuid4() takes 0 args", &callNode); ok=false; return true; }
                out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true;
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
        if (base && base->id == "json") {
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
        if (base && base->id == "re") {
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
        if (base && base->id == "itertools") {
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
        if (base && base->id == "pathlib") {
            if (fn == "cwd" || fn == "home") { if (!callNode.args.empty()) { addDiag(diags, std::string("pathlib.") + fn + "() takes 0 args", &callNode); ok=false; return true; } out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            if (fn == "join") { if (callNode.args.size()!=2) { addDiag(diags, "pathlib.join() takes 2 args", &callNode); ok=false; return true; } ExpressionTyper a{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(a); if (!a.ok) { ok=false; return true; } ExpressionTyper b{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[1]->accept(b); if (!b.ok) { ok=false; return true; } const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str); if ((maskOf(a.out,a.outSet)&~sMask)!=0U || (maskOf(b.out,b.outSet)&~sMask)!=0U) { addDiag(diags, "pathlib.join: arguments must be str", &callNode); ok=false; return true; } out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            if (fn == "parent" || fn == "basename" || fn == "suffix" || fn == "stem" || fn == "as_posix" || fn == "as_uri" || fn == "resolve" || fn == "absolute") { if (callNode.args.size()!=1) { addDiag(diags, std::string("pathlib.")+fn+"() takes 1 arg", &callNode); ok=false; return true; } ExpressionTyper p{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(p); if (!p.ok) { ok=false; return true; } const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str); if ((maskOf(p.out,p.outSet)&~sMask)!=0U) { addDiag(diags, std::string("pathlib.")+fn+": path must be str", callNode.args[0].get()); ok=false; return true; } out = ast::TypeKind::Str; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            if (fn == "exists" || fn == "is_file" || fn == "is_dir" || fn == "match") { const size_t need = (fn=="match"?2u:1u); if (callNode.args.size()!=need) { addDiag(diags, std::string("pathlib.")+fn+ (need==1?"() takes 1 arg":"() takes 2 args"), &callNode); ok=false; return true; } ExpressionTyper p{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[0]->accept(p); if (!p.ok) { ok=false; return true; } const uint32_t sMask = TypeEnv::maskForKind(ast::TypeKind::Str); if ((maskOf(p.out,p.outSet)&~sMask)!=0U) { addDiag(diags, std::string("pathlib.")+fn+": path must be str", callNode.args[0].get()); ok=false; return true; } if (need==2) { ExpressionTyper q{env,sigs,retParamIdxs,diags,polyTargets,outers}; callNode.args[1]->accept(q); if (!q.ok) { ok=false; return true; } if ((maskOf(q.out,q.outSet)&~sMask)!=0U) { addDiag(diags, "pathlib.match: pattern must be str", callNode.args[1].get()); ok=false; return true; } } out = ast::TypeKind::Bool; outSet = TypeEnv::maskForKind(out); const_cast<ast::Call&>(callNode).setType(out); return true; }
            return false;
        }
        return false;
    }
} // namespace pycc::sema::detail
