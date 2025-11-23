/**
 * @file
 * @brief handleBinaryComparison: Type checks comparison binary operators.
 */
#include "sema/detail/exptyper/BinaryHandlers.h"

namespace pycc::sema::detail {
    bool handleBinaryComparison(const ast::Binary &node,
                                const uint32_t lMask,
                                const uint32_t rMask,
                                ast::TypeKind &out,
                                uint32_t &outSet,
                                std::vector<Diagnostic> &diags) {
        using Type = ast::TypeKind;
        const auto op = node.op;
        const bool isCmp = (op == ast::BinaryOperator::Eq || op == ast::BinaryOperator::Ne ||
                            op == ast::BinaryOperator::Lt || op == ast::BinaryOperator::Le ||
                            op == ast::BinaryOperator::Gt || op == ast::BinaryOperator::Ge ||
                            op == ast::BinaryOperator::Is || op == ast::BinaryOperator::IsNot);
        if (!isCmp) return false;
        // Allow eq/ne None comparisons regardless of the other type
        if (op == ast::BinaryOperator::Eq || op == ast::BinaryOperator::Ne) {
            if (auto isNone = [](const uint32_t m) {
                    return m == TypeEnv::maskForKind(Type::NoneType);
                };
                isNone(lMask) || isNone(rMask)) {
                out = Type::Bool;
                outSet = TypeEnv::maskForKind(Type::Bool);
                return true;
            }
        }
        const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
        const uint32_t fMask = TypeEnv::maskForKind(Type::Float);
        if (const uint32_t sMask = TypeEnv::maskForKind(Type::Str); lMask == sMask && rMask == sMask) {
            out = Type::Bool;
            outSet = TypeEnv::maskForKind(Type::Bool);
            return true;
        }
        if ((lMask == iMask && rMask == iMask) || (lMask == fMask && rMask == fMask)) {
            out = Type::Bool;
            outSet = TypeEnv::maskForKind(Type::Bool);
            return true;
        }
        addDiag(diags, "comparison operands must match types (int,int) or (float,float) or (str,str)", &node);
        return true;
    }
} // namespace pycc::sema::detail
