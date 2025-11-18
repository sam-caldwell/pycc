/***
 * Name: pycc::obs::AstPrinter
 * Purpose: Visitor-based AST pretty-printer for diagnostics/logging.
 * Inputs:
 *   - ast::Module
 * Outputs:
 *   - Formatted string with node kinds and salient fields.
 * Theory of Operation:
 *   Implements ast::VisitorBase to traverse nodes, collecting a textual
 *   representation with indentation reflecting tree depth.
 */
#pragma once

#include <string>
#include <sstream>
#include "ast/Nodes.h"
#include "ast/VisitorBase.h"

namespace pycc::obs {

class AstPrinter : public ast::VisitorBase {
 public:
  std::string print(const ast::Module& m) {
    ss_.str(""); ss_.clear(); depth_ = 0;
    m.accept(*this);
    return ss_.str();
  }

  void visit(const ast::Module& m) override { line("Module"); depth_++; for (const auto& f : m.functions) f->accept(*this); for (const auto& c : m.classes) c->accept(*this); depth_--; }
  void visit(const ast::FunctionDef& f) override { line(std::string("FunctionDef name=") + f.name + ", ret=" + ast::to_string(f.returnType)); depth_++; for (const auto& s : f.body) s->accept(*this); depth_--; }
  void visit(const ast::ReturnStmt& r) override { line("ReturnStmt"); depth_++; if (r.value) r.value->accept(*this); depth_--; }
  void visit(const ast::AssignStmt& a) override { line(std::string("AssignStmt target=") + a.target); depth_++; if (a.value) a.value->accept(*this); depth_--; }
  void visit(const ast::ExprStmt& a) override { line("ExprStmt"); depth_++; if (a.value) a.value->accept(*this); depth_--; }
  void visit(const ast::IfStmt& i) override { line("IfStmt"); depth_++; if (i.cond) { line("Cond:"); depth_++; i.cond->accept(*this); depth_--; } if (!i.thenBody.empty()) { line("Then:"); depth_++; for (const auto& s : i.thenBody) s->accept(*this); depth_--; } if (!i.elseBody.empty()) { line("Else:"); depth_++; for (const auto& s : i.elseBody) s->accept(*this); depth_--; } depth_--; }
  void visit(const ast::WhileStmt& w) override { line("WhileStmt"); depth_++; if (w.cond) { line("Cond:"); depth_++; w.cond->accept(*this); depth_--; } if (!w.thenBody.empty()) { line("Then:"); depth_++; for (const auto& s : w.thenBody) s->accept(*this); depth_--; } if (!w.elseBody.empty()) { line("Else:"); depth_++; for (const auto& s : w.elseBody) s->accept(*this); depth_--; } depth_--; }
  void visit(const ast::ForStmt& f) override { line("ForStmt"); depth_++; if (f.target) { line("Target:"); depth_++; f.target->accept(*this); depth_--; } if (f.iterable) { line("Iter:"); depth_++; f.iterable->accept(*this); depth_--; } if (!f.thenBody.empty()) { line("Then:"); depth_++; for (const auto& s : f.thenBody) s->accept(*this); depth_--; } if (!f.elseBody.empty()) { line("Else:"); depth_++; for (const auto& s : f.elseBody) s->accept(*this); depth_--; } depth_--; }
  void visit(const ast::AugAssignStmt& a) override { line("AugAssignStmt"); depth_++; if (a.target) a.target->accept(*this); if (a.value) a.value->accept(*this); depth_--; }
  void visit(const ast::TryStmt& t) override { line("TryStmt"); depth_++; if (!t.body.empty()) { line("Body:"); depth_++; for (const auto& s : t.body) s->accept(*this); depth_--; } for (const auto& h : t.handlers) { if (h) h->accept(*this); } if (!t.orelse.empty()) { line("Else:"); depth_++; for (const auto& s : t.orelse) s->accept(*this); depth_--; } if (!t.finalbody.empty()) { line("Finally:"); depth_++; for (const auto& s : t.finalbody) s->accept(*this); depth_--; } depth_--; }
  void visit(const ast::ExceptHandler& h) override { line("ExceptHandler"); depth_++; if (h.type) { h.type->accept(*this); } for (const auto& s : h.body) { s->accept(*this); } depth_--; }
  void visit(const ast::ClassDef& c) override { line(std::string("ClassDef name=") + c.name); depth_++; for (const auto& s : c.body) s->accept(*this); depth_--; }
  void visit(const ast::ListComp& n) override { line("ListComp"); depth_++; if (n.elt) n.elt->accept(*this); depth_--; }
  void visit(const ast::SetComp& n) override { line("SetComp"); depth_++; if (n.elt) n.elt->accept(*this); depth_--; }
  void visit(const ast::DictComp& n) override { line("DictComp"); depth_++; if (n.key) n.key->accept(*this); if (n.value) n.value->accept(*this); depth_--; }
  void visit(const ast::GeneratorExpr& n) override { line("GeneratorExpr"); depth_++; if (n.elt) n.elt->accept(*this); depth_--; }
  void visit(const ast::MatchStmt& m) override { line("MatchStmt"); (void)m; }
  void visit(const ast::IntLiteral& lit) override { line(std::string("IntLiteral ") + std::to_string(static_cast<int>(lit.value))); }
  void visit(const ast::BoolLiteral& lit) override { line(std::string("BoolLiteral ") + (lit.value ? "True" : "False")); }
  void visit(const ast::FloatLiteral& lit) override { line(std::string("FloatLiteral ") + std::to_string(lit.value)); }
  void visit(const ast::StringLiteral& lit) override { line(std::string("StringLiteral \"") + lit.value + "\""); }
  void visit(const ast::NoneLiteral&) override { line("NoneLiteral"); }
  void visit(const ast::Name& n) override { line(std::string("Name ") + n.id); }
  void visit(const ast::Call& c) override { line("Call"); depth_++; if (c.callee) c.callee->accept(*this); for (const auto& a : c.args) a->accept(*this); depth_--; }
  void visit(const ast::Binary& b) override { line("Binary"); depth_++; if (b.lhs) b.lhs->accept(*this); if (b.rhs) b.rhs->accept(*this); depth_--; }
  void visit(const ast::Unary& u) override { line("Unary"); depth_++; if (u.operand) u.operand->accept(*this); depth_--; }
  void visit(const ast::TupleLiteral& t) override { line("TupleLiteral"); depth_++; for (const auto& e : t.elements) e->accept(*this); depth_--; }
  void visit(const ast::ListLiteral& t) override { line("ListLiteral"); depth_++; for (const auto& e : t.elements) e->accept(*this); depth_--; }
  void visit(const ast::ObjectLiteral& t) override { line("ObjectLiteral"); depth_++; for (const auto& e : t.fields) e->accept(*this); depth_--; }

 private:
  void indent() { for (int i = 0; i < depth_; ++i) ss_ << "  "; }
  void line(const std::string& s) { indent(); ss_ << s << "\n"; }
  std::ostringstream ss_{};
  int depth_{0};
};

} // namespace pycc::obs
