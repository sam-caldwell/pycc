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
        return false;
    }
} // namespace pycc::sema::detail
