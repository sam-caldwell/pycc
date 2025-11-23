/**
 * @file
 * @brief handleBinaryArithmetic: Type checks arithmetic binary operators.
 */
#include "sema/detail/exptyper/BinaryHandlers.h"

namespace pycc::sema::detail {
    bool handleBinaryArithmetic(const ast::Binary &node,
                                const uint32_t lMask,
                                const uint32_t rMask,
                                ast::TypeKind &out,
                                uint32_t &outSet,
                                std::vector<Diagnostic> &diags) {
        using Type = ast::TypeKind;
        const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
        const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
        const uint32_t sMask = TypeEnv::maskForKind(Type::Str);

        const auto op = node.op;
        const bool isArithmetic = (op == ast::BinaryOperator::Add || op == ast::BinaryOperator::Sub ||
                                   op == ast::BinaryOperator::Mul || op == ast::BinaryOperator::Div ||
                                   op == ast::BinaryOperator::Mod || op == ast::BinaryOperator::FloorDiv ||
                                   op == ast::BinaryOperator::Pow);
        if (!isArithmetic) return false;

        if (op == ast::BinaryOperator::Add && lMask == sMask && rMask == sMask) {
            out = Type::Str;
            outSet = sMask;
            return true;
        }
        if (lMask == iMask && rMask == iMask) {
            out = Type::Int;
            outSet = iMask;
            return true;
        }
        if (op != ast::BinaryOperator::Mod && lMask == fMask && rMask == fMask) {
            out = Type::Float;
            outSet = fMask;
            return true;
        }

        const uint32_t numMask = iMask | fMask;
        if (auto subMask = [](const uint32_t msk, const uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
            subMask(lMask, numMask) && subMask(rMask, numMask)) {

            addDiag(diags, "ambiguous numeric types; both operands must be int or both float", &node);

        } else {
            addDiag(diags, "arithmetic operands must both be int or both be float (mod only for int)", &node);
        }
        return true; // handled with error
    }
} // namespace pycc::sema::detail
