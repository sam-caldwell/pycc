/***
 * Name: pycc::opt::Optimizer (impl)
 * Purpose: Visitor-based traversal for future optimization passes.
 */
#include "optimizer/Optimizer.h"
#include "ast/VisitorBase.h"

namespace pycc::opt {

struct Counter : public ast::VisitorBase {
  Stats s;
  void visit(const ast::Module& m) override { ++s.nodesVisited; for (const auto& f : m.functions) f->accept(*this); }
  void visit(const ast::FunctionDef& f) override { ++s.nodesVisited; for (const auto& st : f.body) st->accept(*this); }
  void visit(const ast::ReturnStmt& r) override { ++s.nodesVisited; ++s.stmtsVisited; if (r.value) r.value->accept(*this); }
  void visit(const ast::AssignStmt& a) override { ++s.nodesVisited; ++s.stmtsVisited; if (a.value) a.value->accept(*this); }
  void visit(const ast::IfStmt& i) override { ++s.nodesVisited; ++s.stmtsVisited; if (i.cond) i.cond->accept(*this); for (const auto& s2 : i.thenBody) s2->accept(*this); for (const auto& s3 : i.elseBody) s3->accept(*this); }
  void visit(const ast::ExprStmt& e) override { ++s.nodesVisited; ++s.stmtsVisited; if (e.value) e.value->accept(*this); }
  void visit(const ast::IntLiteral&) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit(const ast::BoolLiteral&) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit(const ast::FloatLiteral&) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit(const ast::StringLiteral&) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit(const ast::NoneLiteral&) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit(const ast::Name&) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit(const ast::Call& c) override { ++s.nodesVisited; ++s.exprsVisited; if (c.callee) c.callee->accept(*this); for (const auto& a : c.args) a->accept(*this); }
  void visit(const ast::Binary& b) override { ++s.nodesVisited; ++s.exprsVisited; if (b.lhs) b.lhs->accept(*this); if (b.rhs) b.rhs->accept(*this); }
  void visit(const ast::Unary& u) override { ++s.nodesVisited; ++s.exprsVisited; if (u.operand) u.operand->accept(*this); }
  void visit(const ast::TupleLiteral& t) override { ++s.nodesVisited; ++s.exprsVisited; for (const auto& e : t.elements) e->accept(*this); }
  void visit(const ast::ListLiteral& t) override { ++s.nodesVisited; ++s.exprsVisited; for (const auto& e : t.elements) e->accept(*this); }
};

Stats Optimizer::analyze(const ast::Module& m) const {
  Counter c; m.accept(c); return c.s;
}

} // namespace pycc::opt
