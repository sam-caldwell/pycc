/***
 * Name: computeReturnParamIdxs (definition)
 * Purpose: Build a map of functions that consistently return one of their
 *          parameters, mapping function name -> returned parameter index.
 */
#include "sema/detail/ReturnParamScan.h"

namespace pycc::sema {
    std::unordered_map<std::string, int> computeReturnParamIdxs(const ast::Module &mod) {
        struct RetIdxVisitor : public ast::VisitorBase {
            const ast::FunctionDef *fn{nullptr};
            int retIdx{-1};
            bool hasReturn{false};
            bool consistent{true};

            void visit(const ast::ReturnStmt &ret) override {
                if (!consistent) return;
                hasReturn = true;
                if (!(ret.value && ret.value->kind == ast::NodeKind::Name)) {
                    consistent = false;
                    return;
                }
                const auto *nameNode = static_cast<const ast::Name *>(ret.value.get());
                int idxFound = -1;
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    if (fn->params[i].name == nameNode->id) {
                        idxFound = static_cast<int>(i);
                        break;
                    }
                }
                if (idxFound < 0) {
                    consistent = false;
                    return;
                }
                if (retIdx < 0) retIdx = idxFound;
                else if (retIdx != idxFound) consistent = false;
            }

            void visit(const ast::IfStmt &iff) override {
                for (const auto &stmt: iff.thenBody) { stmt->accept(*this); }
                for (const auto &stmt: iff.elseBody) { stmt->accept(*this); }
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Module &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::FunctionDef &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::AssignStmt &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::ExprStmt &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral> &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral> &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral> &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral> &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::NoneLiteral &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Name &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Call &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Binary &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::Unary &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::TupleLiteral &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::ListLiteral &) override {
                //noop
            }

            // required pure-virtuals (no-ops)
            void visit(const ast::ObjectLiteral &) override {
                //noop
            }
        };
        std::unordered_map<std::string, int> retParamIdxs;
        for (const auto &func: mod.functions) {
            RetIdxVisitor v;
            v.fn = func.get();
            for (const auto &stmt: func->body) {
                stmt->accept(v);
                if (!v.consistent) break;
            }
            if (v.hasReturn && v.consistent && v.retIdx >= 0) retParamIdxs[func->name] = v.retIdx;
        }
        return retParamIdxs;
    }
} // namespace pycc::sema
