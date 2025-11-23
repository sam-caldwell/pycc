/***
 * Name: scanFunctionTraits (definition)
 * Purpose: Pre-scan functions for generator/coroutine traits (yield/await)
 *          and populate the flags map.
 */
#include "sema/detail/FnTraitScan.h"

namespace pycc::sema {
    void scanFunctionTraits(const ast::Module &mod,
                            std::unordered_map<const ast::FunctionDef *, FuncFlags> &out) {
        struct FnTraitScan : public ast::VisitorBase {
            bool hasYield{false};
            bool hasAwait{false};

            void visit(const ast::Module &) override {
            }

            void visit(const ast::FunctionDef &) override {
            }

            void visit(const ast::YieldExpr &) override { hasYield = true; }
            void visit(const ast::AwaitExpr &) override { hasAwait = true; }
            void visit(const ast::ReturnStmt &rs) override { if (rs.value) rs.value->accept(*this); }

            void visit(const ast::AssignStmt &) override {
            }

            void visit(const ast::IfStmt &is) override {
                if (is.cond) is.cond->accept(*this);
                for (const auto &s: is.thenBody) if (s) s->accept(*this);
                for (const auto &s: is.elseBody) if (s) s->accept(*this);
            }

            void visit(const ast::ExprStmt &es) override { if (es.value) es.value->accept(*this); }
            // pure-virtual stubs
            void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral> &) override {
            }

            void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral> &) override {
            }

            void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral> &) override {
            }

            void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral> &) override {
            }

            void visit(const ast::NoneLiteral &) override {
            }

            void visit(const ast::Name &) override {
            }

            void visit(const ast::Call &) override {
            }

            void visit(const ast::Binary &) override {
            }

            void visit(const ast::Unary &) override {
            }

            void visit(const ast::TupleLiteral &) override {
            }

            void visit(const ast::ListLiteral &) override {
            }

            void visit(const ast::ObjectLiteral &) override {
            }
        };
        for (const auto &func: mod.functions) {
            FnTraitScan scan;
            for (const auto &st: func->body) if (st) st->accept(scan);
            FuncFlags flags;
            flags.isGenerator = scan.hasYield;
            flags.isCoroutine = scan.hasAwait;
            out[func.get()] = flags;
        }
    }
} // namespace pycc::sema
