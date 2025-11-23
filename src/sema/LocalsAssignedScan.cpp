/***
 * Name: scanLocalsAssigned (definition)
 * Purpose: Traverse a function body and collect all simple local names that
 *          receive assignments (including augmented and for-targets).
 */
#include "sema/detail/LocalsAssignedScan.h"
#include "ast/AssignStmt.h"
#include "ast/AugAssignStmt.h"
#include "ast/ForStmt.h"
#include "ast/IfStmt.h"
#include "ast/TryStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ReturnStmt.h"
#include "ast/ExprStmt.h"
#include "ast/TupleLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Name.h"
#include "ast/VisitorBase.h"

namespace pycc::sema::detail {
    namespace {
        struct ScanVisitor final : public ast::VisitorBase {
            std::unordered_set<std::string> &out;

            explicit ScanVisitor(std::unordered_set<std::string> &o) : out(o) {
            }

            void addTarget(const ast::Expr *e) {
                if (!e) return;
                switch (e->kind) {
                    case ast::NodeKind::Name: {
                        const auto *n = static_cast<const ast::Name *>(e);
                        out.insert(n->id);
                        break;
                    }
                    case ast::NodeKind::TupleLiteral: {
                        for (const auto *t = static_cast<const ast::TupleLiteral *>(e); const auto &el: t->elements) {
                            if (el) addTarget(el.get());
                        }
                        break;
                    }
                    case ast::NodeKind::ListLiteral: {
                        for (const auto *l = static_cast<const ast::ListLiteral *>(e); const auto &el: l->elements) {
                            if (el) addTarget(el.get());
                        }
                        break;
                    }
                    default: break; // attribute/subscript do not introduce a new local name
                }
            }

            void visit(const ast::AssignStmt &as) override {
                if (!as.targets.empty()) {
                    for (const auto &t: as.targets) { if (t) addTarget(t.get()); }
                } else if (!as.target.empty()) {
                    out.insert(as.target);
                }
            }

            void visit(const ast::AugAssignStmt &as) override { addTarget(as.target.get()); }

            void visit(const ast::ForStmt &fs) override {
                addTarget(fs.target.get());
                for (const auto &s: fs.thenBody) if (s) s->accept(*this);
                for (const auto &s: fs.elseBody) if (s) s->accept(*this);
            }

            void visit(const ast::IfStmt &is) override {
                for (const auto &s: is.thenBody)
                    if (s)
                        s->accept(*this);
                for (const auto &s: is.elseBody)
                    if (s)
                        s->accept(*this);
            }

            void visit(const ast::WhileStmt &ws) override {
                for (const auto &s: ws.thenBody)
                    if (s)
                        s->accept(*this);
                for (const auto &s: ws.elseBody)
                    if (s)
                        s->accept(*this);
            }

            void visit(const ast::TryStmt &ts) override {
                for (const auto &s: ts.body)
                    if (s)
                        s->accept(*this);
                for (const auto &h: ts.handlers)
                    if (h) for (const auto &s: h->body)
                        if (s)
                            s->accept(*this);
                for (const auto &s: ts.orelse)
                    if (s)
                        s->accept(*this);
                for (const auto &s: ts.finalbody)
                    if (s)
                        s->accept(*this);
            }

            // do not recurse into nested function bodies
            void visit(const ast::FunctionDef &) override {
            }

            // non-assignment statements: no-op
            void visit(const ast::Module &) override {
            }

            void visit(const ast::ExprStmt &) override {
            }

            void visit(const ast::ReturnStmt &) override {
            }

            void visit(const ast::Name &) override {
            }

            void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral> &) override {
            }

            void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral> &) override {
            }

            void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral> &) override {
            }

            void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral> &) override {
            }

            void visit(const ast::Binary &) override {
            }

            void visit(const ast::Unary &) override {
            }

            void visit(const ast::Call &) override {
            }

            void visit(const ast::TupleLiteral &) override {
            }

            void visit(const ast::ListLiteral &) override {
            }

            void visit(const ast::ObjectLiteral &) override {
            }

            void visit(const ast::NoneLiteral &) override {
            }
        };
    } // namespace

    void scanLocalsAssigned(const ast::FunctionDef &fn, std::unordered_set<std::string> &out) {
        ScanVisitor v{out};
        for (const auto &st: fn.body) if (st) st->accept(v);
    }
} // namespace pycc::sema::detail
