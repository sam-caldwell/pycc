/***
 * Name: scanFunctionTraits
 * Purpose: Mark functions that contain yield/await for flags.
 */
#include "sema/detail/FnTraitScan.h"
#include "ast/VisitorBase.h"
#include "ast/FunctionDef.h"
#include "ast/ReturnStmt.h"
#include "ast/IfStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ForStmt.h"
#include "ast/TryStmt.h"
#include "ast/ExceptHandler.h"
#include "ast/ExprStmt.h"
#include "ast/YieldExpr.h"
#include "ast/AwaitExpr.h"

using namespace pycc;
using namespace pycc::sema;

namespace {
struct FnTraitScan : public ast::VisitorBase {
  bool hasYield{false}; bool hasAwait{false};
  void visit(const ast::Module&) override {}
  void visit(const ast::FunctionDef&) override {}
  // Satisfy required pure virtuals with no-ops for nodes we don't care about
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
  void visit(const ast::YieldExpr&) override { hasYield = true; }
  void visit(const ast::AwaitExpr&) override { hasAwait = true; }
  void visit(const ast::AssignStmt&) override {}
  void visit(const ast::ExprStmt& es) override { if (es.value) es.value->accept(*this); }
  void visit(const ast::ReturnStmt& rs) override { if (rs.value) rs.value->accept(*this); }
  void visit(const ast::IfStmt& is) override { if (is.cond) is.cond->accept(*this); for (const auto& s : is.thenBody) if (s) s->accept(*this); for (const auto& s : is.elseBody) if (s) s->accept(*this); }
  void visit(const ast::WhileStmt& ws) override { if (ws.cond) ws.cond->accept(*this); for (const auto& s : ws.thenBody) if (s) s->accept(*this); for (const auto& s : ws.elseBody) if (s) s->accept(*this); }
  void visit(const ast::ForStmt& fs) override { if (fs.target) fs.target->accept(*this); if (fs.iterable) fs.iterable->accept(*this); for (const auto& s : fs.thenBody) if (s) s->accept(*this); for (const auto& s : fs.elseBody) if (s) s->accept(*this); }
  void visit(const ast::TryStmt& ts) override { for (const auto& s : ts.body) if (s) s->accept(*this); for (const auto& h : ts.handlers) if (h) { for (const auto& s : h->body) if (s) s->accept(*this); } for (const auto& s : ts.orelse) if (s) s->accept(*this); for (const auto& s : ts.finalbody) if (s) s->accept(*this); }
};
} // namespace

void scanFunctionTraits(const ast::Module& mod,
                        std::unordered_map<const ast::FunctionDef*, Sema::FuncFlags>& out) {
  for (const auto& func : mod.functions) {
    FnTraitScan scan; for (const auto& st : func->body) if (st) st->accept(scan);
    Sema::FuncFlags flags; flags.isGenerator = scan.hasYield; flags.isCoroutine = scan.hasAwait;
    out[func.get()] = flags;
  }
}
