/**
 * @file
 * @brief handleBinaryBitwise: Type checks bitwise and shift binary operators.
 */
#include "sema/detail/exptyper/BinaryHandlers.h"

namespace pycc::sema::detail {
    bool handleBinaryBitwise(const ast::Binary &node,
                             const uint32_t lMask,
                             const uint32_t rMask,
                             ast::TypeKind &out,
                             uint32_t &outSet,
                             std::vector<Diagnostic> &diags) {
        using Type = ast::TypeKind;
        const uint32_t iMask = TypeEnv::maskForKind(Type::Int);
        const auto op = node.op;
        const bool isBitwise = (op == ast::BinaryOperator::BitAnd || op == ast::BinaryOperator::BitOr ||
                                op == ast::BinaryOperator::BitXor || op == ast::BinaryOperator::LShift ||
                                op == ast::BinaryOperator::RShift);

        if (!isBitwise) return false;

        if (lMask == iMask && rMask == iMask) {
            out = Type::Int;
            outSet = iMask;
            return true;
        }
        addDiag(diags, "bitwise/shift operands must be int", &node);
        return true;
    }
} // namespace pycc::sema::detail
