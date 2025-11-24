/***
 * @file
 * @brief Forward declaration of ExpressionTyper visitor used for expression typing.
 */
#pragma once

#include "ast/VisitorBase.h"
#include "ast/TypeKind.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/detail/Helpers.h"

#include <unordered_map>
#include <vector>

namespace pycc::sema {
    /***
     * @class ExpressionTyper
     * @brief AST visitor that infers expression types and canonical forms.
     */
    class ExpressionTyper final : public ast::VisitorBase {
    public:
        // Back-compat constructor without classes map
        ExpressionTyper(const TypeEnv &env_, const std::unordered_map<std::string, Sig> &sigs_,
                        const std::unordered_map<std::string, int> &retParamIdxs_, std::vector<Diagnostic> &diags_,
                        PolyPtrs polyIn = {}, const std::vector<const TypeEnv *> *outerScopes_ = nullptr);

        // Extended constructor with classes map
        ExpressionTyper(const TypeEnv &env_, const std::unordered_map<std::string, Sig> &sigs_,
                        const std::unordered_map<std::string, int> &retParamIdxs_, std::vector<Diagnostic> &diags_,
                        PolyPtrs polyIn, const std::vector<const TypeEnv *> *outerScopes_,
                        const std::unordered_map<std::string, ClassInfo> *classes_);

        // Outputs
        ast::TypeKind out{ast::TypeKind::NoneType};
        uint32_t outSet{0};
        bool ok{true};

        // Visitor overrides
        void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral> &) override;

        void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral> &) override;

        void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral> &) override;

        void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral> &) override;
        void visit(const ast::Literal<std::string, ast::NodeKind::BytesLiteral> &) override;

        void visit(const ast::NoneLiteral &) override;

        void visit(const ast::Attribute &) override;

        void visit(const ast::Subscript &) override;

        void visit(const ast::ObjectLiteral &) override;

        void visit(const ast::Name &) override;

        void visit(const ast::Unary &) override;

        void visit(const ast::Binary &) override;

        void visit(const ast::ExprStmt &) override; // error path
        void visit(const ast::TupleLiteral &) override;

        void visit(const ast::ListLiteral &) override;

        void visit(const ast::SetLiteral &) override;

        void visit(const ast::DictLiteral &) override;

        void visit(const ast::ListComp &) override;

        void visit(const ast::SetComp &) override;

        void visit(const ast::DictComp &) override;

        void visit(const ast::YieldExpr &) override;

        void visit(const ast::AwaitExpr &) override;

        void visit(const ast::GeneratorExpr &) override;

        void visit(const ast::IfExpr &) override;

        void visit(const ast::Call &) override;

        // Error paths for statement nodes encountered as expressions
        void visit(const ast::ReturnStmt &) override;

        void visit(const ast::AssignStmt &) override;

        void visit(const ast::IfStmt &) override;

        void visit(const ast::FunctionDef &) override;

        void visit(const ast::Module &) override;

    private:
        const TypeEnv *env{nullptr};
        const std::unordered_map<std::string, Sig> *sigs{nullptr};
        const std::unordered_map<std::string, int> *retParamIdxs{nullptr};
        std::vector<Diagnostic> *diags{nullptr};
        PolyPtrs polyTargets{};
        const std::vector<const TypeEnv *> *outers{nullptr};
        const std::unordered_map<std::string, ClassInfo> *classes{nullptr};
    };
} // namespace pycc::sema
