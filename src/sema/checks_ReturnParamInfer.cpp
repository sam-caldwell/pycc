/**
 * @file
 * @brief inferReturnParamIdx: Determine if a function always returns one of its params.
 */
#include "sema/detail/checks/ReturnParamInfer.h"
#include "ast/Nodes.h"

namespace pycc::sema::detail {

std::optional<int> inferReturnParamIdx(const ast::FunctionDef& fn) {
    struct RetIdxVisitor : public ast::VisitorBase {
        const ast::FunctionDef* fn{nullptr};
        int retIdx{-1};
        bool hasReturn{false};
        bool consistent{true};

        void visit(const ast::ReturnStmt& ret) override {
            if (!consistent) return; hasReturn = true;
            if (!(ret.value && ret.value->kind == ast::NodeKind::Name)) { consistent = false; return; }
            const auto* nameNode = static_cast<const ast::Name*>(ret.value.get());
            int idxFound = -1;
            for (size_t i = 0; i < fn->params.size(); ++i) { if (fn->params[i].name == nameNode->id) { idxFound = static_cast<int>(i); break; } }
            if (idxFound < 0) { consistent = false; return; }
            if (retIdx < 0) retIdx = idxFound; else if (retIdx != idxFound) consistent = false;
        }
        void visit(const ast::IfStmt& iff) override { for (const auto& s : iff.thenBody) s->accept(*this); for (const auto& s : iff.elseBody) s->accept(*this); }
        // required pure-virtuals (no-ops)
        void visit(const ast::Module&) override {}
        void visit(const ast::FunctionDef&) override {}
        void visit(const ast::AssignStmt&) override {}
        void visit(const ast::ExprStmt&) override {}
        void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>&) override {}
        void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>&) override {}
        void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>&) override {}
        void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>&) override {}
        void visit(const ast::NoneLiteral&) override {}
        void visit(const ast::Name&) override {}
        void visit(const ast::Call&) override {}
        void visit(const ast::Binary&) override {}
        void visit(const ast::Unary&) override {}
        void visit(const ast::TupleLiteral&) override {}
        void visit(const ast::ListLiteral&) override {}
        void visit(const ast::ObjectLiteral&) override {}
    };

    RetIdxVisitor v; v.fn = &fn; for (const auto& stmt : fn.body) { stmt->accept(v); if (!v.consistent) break; }
    if (v.hasReturn && v.consistent && v.retIdx >= 0) return v.retIdx; return std::nullopt;
}

} // namespace pycc::sema::detail
