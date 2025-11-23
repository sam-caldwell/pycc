/**
 * @file
 * @brief handleBinaryLogical: Type checks logical binary operators.
 */
#include "sema/detail/exptyper/BinaryHandlers.h"

namespace pycc::sema::detail {
    bool handleBinaryLogical(const ast::Binary &node,
                             const uint32_t lMask,
                             const uint32_t rMask,
                             ast::TypeKind &out,
                             uint32_t &outSet,
                             std::vector<Diagnostic> &diags) {
        using Type = ast::TypeKind;
        const auto op = node.op;
        if (const bool isLogical = (op == ast::BinaryOperator::And || op == ast::BinaryOperator::Or); !isLogical)
            return
                    false;
        const uint32_t bMask = TypeEnv::maskForKind(Type::Bool);
        if (auto isSubset = [](const uint32_t msk, const uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
            !isSubset(lMask, bMask) || !isSubset(rMask, bMask)) {
            addDiag(diags, "logical operands must be bool", &node);
            return true;
        }
        out = Type::Bool;
        outSet = bMask;
        return true;
    }
} // namespace pycc::sema::detail
