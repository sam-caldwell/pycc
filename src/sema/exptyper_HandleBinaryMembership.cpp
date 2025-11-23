/**
 * @file
 * @brief handleBinaryMembership: Type checks membership binary operators.
 */
#include "sema/detail/exptyper/BinaryHandlers.h"
#include "sema/detail/ExpressionTyper.h"
#include "ast/ListLiteral.h"

namespace pycc::sema::detail {
    static inline uint32_t typeMask(ast::TypeKind k, uint32_t set) {
        return set != 0U ? set : TypeEnv::maskForKind(k);
    }

    bool handleBinaryMembership(const ast::Binary &node,
                                const uint32_t lMask,
                                const uint32_t rMask,
                                const TypeEnv &env,
                                const std::unordered_map<std::string, Sig> &sigs,
                                const std::unordered_map<std::string, int> &retParamIdxs,
                                std::vector<Diagnostic> &diags,
                                const PolyPtrs poly,
                                const std::vector<const TypeEnv *> *outers,
                                ast::TypeKind &out,
                                uint32_t &outSet) {
        using Type = ast::TypeKind;
        if (const auto op = node.op; !(op == ast::BinaryOperator::In || op == ast::BinaryOperator::NotIn))
            return false;

        const uint32_t sMask = TypeEnv::maskForKind(Type::Str);
        const uint32_t listMask = TypeEnv::maskForKind(Type::List);
        auto isSubset = [](const uint32_t msk, const uint32_t allow) {
            return msk && ((msk & ~allow) == 0U);
        };

        if (rMask == sMask) {
            if (!isSubset(lMask, sMask)) {
                addDiag(diags, "left operand must be str when right is str for 'in'", &node);
                return true;
            }
            out = Type::Bool;
            outSet = TypeEnv::maskForKind(Type::Bool);
            return true;
        }
        if (rMask == listMask || node.rhs->kind == ast::NodeKind::ListLiteral) {
            uint32_t elemMask = 0U;
            if (node.rhs->kind == ast::NodeKind::Name) {
                const auto *nm = static_cast<const ast::Name *>(node.rhs.get());
                elemMask = env.getListElems(nm->id);
            } else if (node.rhs->kind == ast::NodeKind::ListLiteral) {
                for (const auto *lst = static_cast<const ast::ListLiteral *>(node.rhs.get()); const auto &el: lst->
                     elements) {
                    if (!el) continue;
                    ExpressionTyper et{env, sigs, retParamIdxs, diags, poly, outers};
                    el->accept(et);
                    if (!et.ok) { return true; }
                    elemMask |= typeMask(et.out, et.outSet);
                }
            }
            if (elemMask != 0U) {
                if (!isSubset(lMask, elemMask)) {
                    addDiag(diags, "left operand not permitted for membership in list", &node);
                    return true;
                }
                out = Type::Bool;
                outSet = TypeEnv::maskForKind(Type::Bool);
                return true;
            }
        }
        addDiag(diags, "unsupported membership target", &node);
        return true;
    }
} // namespace pycc::sema::detail
