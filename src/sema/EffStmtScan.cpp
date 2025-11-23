/***
 * Name: scanStmtEffects (definition)
 * Purpose: Populate per-statement mayRaise flags across all functions using
 *          the EffectsScan expression visitor.
 */
#include "sema/detail/EffStmtScan.h"

namespace pycc::sema {

void scanStmtEffects(const ast::Module& mod,
                     std::unordered_map<const ast::Stmt*, bool>& out) {
  struct EffStmtScan : public ast::VisitorBase {
    std::unordered_map<const ast::Stmt*, bool>& out;
    explicit EffStmtScan(std::unordered_map<const ast::Stmt*, bool>& o) : out(o) {}
    void visit(const ast::Module&) override {}
    void visit(const ast::FunctionDef&) override {}
    void visit(const ast::ExprStmt& es) override { EffectsScan eff; if (es.value) es.value->accept(eff); out[&es] = eff.mayRaise; }
    void visit(const ast::ReturnStmt& rs) override { EffectsScan eff; if (rs.value) rs.value->accept(eff); out[&rs] = eff.mayRaise; }
    void visit(const ast::AssignStmt& as) override { EffectsScan eff; if (as.value) as.value->accept(eff); out[&as] = eff.mayRaise; }
    void visit(const ast::RaiseStmt& rs) override { (void)rs; out[&rs] = true; }
    void visit(const ast::IfStmt& iff) override {
      bool mr=false; EffectsScan e; if (iff.cond) iff.cond->accept(e); mr = e.mayRaise;
      for (const auto& s: iff.thenBody){ EffStmtScan sub{out}; if (s) s->accept(sub); mr = mr || out[s.get()]; }
      for (const auto& s: iff.elseBody){ EffStmtScan sub2{out}; if (s) s->accept(sub2); mr = mr || out[s.get()]; }
      out[&iff] = mr;
    }
    void visit(const ast::WhileStmt& ws) override {
      bool mr=false; EffectsScan e; if (ws.cond) ws.cond->accept(e); mr = e.mayRaise;
      for (const auto& s: ws.thenBody){ EffStmtScan sub{out}; if (s) s->accept(sub); mr = mr || out[s.get()]; }
      for (const auto& s: ws.elseBody){ EffStmtScan sub2{out}; if (s) s->accept(sub2); mr = mr || out[s.get()]; }
      out[&ws] = mr;
    }
    void visit(const ast::ForStmt& fs) override {
      bool mr=false; EffectsScan e; if (fs.iterable) fs.iterable->accept(e); mr = e.mayRaise;
      for (const auto& s: fs.thenBody){ EffStmtScan sub{out}; if (s) s->accept(sub); mr = mr || out[s.get()]; }
      for (const auto& s: fs.elseBody){ EffStmtScan sub2{out}; if (s) s->accept(sub2); mr = mr || out[s.get()]; }
      out[&fs] = mr;
    }
    // stubs
    void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>&) override {}
    void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>&) override {}
    void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>&) override {}
    void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>&) override {}
    void visit(const ast::Name&) override {}
    void visit(const ast::Call&) override {}
    void visit(const ast::Binary&) override {}
    void visit(const ast::Unary&) override {}
    void visit(const ast::TupleLiteral&) override {}
    void visit(const ast::ListLiteral&) override {}
    void visit(const ast::ObjectLiteral&) override {}
    void visit(const ast::NoneLiteral&) override {}
  };
  for (const auto& func : mod.functions) { EffStmtScan ess{out}; for (const auto& st : func->body) if (st) st->accept(ess); }
}

} // namespace pycc::sema

