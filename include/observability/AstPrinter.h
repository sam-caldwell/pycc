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

  void visit(const ast::Module& m) override { line("Module"); depth_++; for (const auto& f : m.functions) f->accept(*this); depth_--; }
  void visit(const ast::FunctionDef& f) override { line(std::string("FunctionDef name=") + f.name + ", ret=" + ast::to_string(f.returnType)); depth_++; for (const auto& s : f.body) s->accept(*this); depth_--; }
  void visit(const ast::ReturnStmt& r) override { line("ReturnStmt"); depth_++; if (r.value) r.value->accept(*this); depth_--; }
  void visit(const ast::AssignStmt& a) override { line(std::string("AssignStmt target=") + a.target); depth_++; if (a.value) a.value->accept(*this); depth_--; }
  void visit(const ast::ExprStmt& a) override { line("ExprStmt"); depth_++; if (a.value) a.value->accept(*this); depth_--; }
  void visit(const ast::IfStmt& i) override { line("IfStmt"); depth_++; if (i.cond) { line("Cond:"); depth_++; i.cond->accept(*this); depth_--; } if (!i.thenBody.empty()) { line("Then:"); depth_++; for (const auto& s : i.thenBody) s->accept(*this); depth_--; } if (!i.elseBody.empty()) { line("Else:"); depth_++; for (const auto& s : i.elseBody) s->accept(*this); depth_--; } depth_--; }
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
