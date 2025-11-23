/**
 * @file
 * @brief handleUnary: Type-check unary operators '-', '~', and 'not'.
 */
#include "sema/detail/exptyper/UnaryHandlers.h"
#include "sema/detail/ExpressionTyper.h"

namespace pycc::sema::detail {

void handleUnary(const ast::Unary& node,
                 const TypeEnv& env,
                 const std::unordered_map<std::string, Sig>& sigs,
                 const std::unordered_map<std::string, int>& retParamIdxs,
                 std::vector<Diagnostic>& diags,
                 PolyPtrs polyTargets,
                 const std::vector<const TypeEnv*>* outers,
                 const std::unordered_map<std::string, ClassInfo>* classes,
                 ast::TypeKind& out,
                 uint32_t& outSet,
                 bool& ok) {
    (void)outers; (void)classes; outSet = 0U;
    const ast::Expr* operandExpr = node.operand.get();
    if (operandExpr == nullptr) { addDiag(diags, "null operand", &node); ok = false; return; }
    ExpressionTyper sub{env, sigs, retParamIdxs, diags, polyTargets};
    operandExpr->accept(sub);
    if (!sub.ok) { ok = false; return; }
    const uint32_t iMask = TypeEnv::maskForKind(ast::TypeKind::Int);
    const uint32_t fMask = TypeEnv::maskForKind(ast::TypeKind::Float);
    const uint32_t bMask = TypeEnv::maskForKind(ast::TypeKind::Bool);
    auto isSubset = [](uint32_t msk, uint32_t allow) { return msk && ((msk & ~allow) == 0U); };
    const uint32_t maskVal = (sub.outSet != 0U) ? sub.outSet : TypeEnv::maskForKind(sub.out);
    if (node.op == ast::UnaryOperator::Neg) {
        if (isSubset(maskVal, iMask)) { out = ast::TypeKind::Int; outSet = iMask; }
        else if (isSubset(maskVal, fMask)) { out = ast::TypeKind::Float; outSet = fMask; }
        else { addDiag(diags, "unary '-' requires int or float", &node); ok = false; return; }
        node.setType(out);
        if (node.operand) { const auto &can = node.operand->canonical(); if (can) node.setCanonicalKey("u:neg:(" + *can + ")"); }
        return;
    }
    if (node.op == ast::UnaryOperator::BitNot) {
        if (!isSubset(maskVal, iMask)) { addDiag(diags, "bitwise '~' requires int", &node); ok = false; return; }
        out = ast::TypeKind::Int; outSet = iMask; node.setType(out);
        if (node.operand) { const auto &can = node.operand->canonical(); if (can) node.setCanonicalKey("u:bitnot:(" + *can + ")"); }
        return;
    }
    if (!isSubset(maskVal, bMask)) { addDiag(diags, "'not' requires bool", &node); ok = false; return; }
    out = ast::TypeKind::Bool; outSet = bMask; node.setType(out);
    if (node.operand) { const auto &can2 = node.operand->canonical(); if (can2) node.setCanonicalKey("u:not:(" + *can2 + ")"); }
}

} // namespace pycc::sema::detail

