/***
 * Name: pycc::opt::DCE (impl)
 */
#include "optimizer/DCE.h"
#include "ast/VisitorBase.h"

namespace pycc::opt {
using pycc::ast::Module;
using pycc::ast::Stmt;
using pycc::ast::IfStmt;
using pycc::ast::NodeKind;

namespace {
struct DceVisitor : public ast::VisitorBase {
  size_t removed{0};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  void touch(std::unique_ptr<Stmt>& s) {
    if (!s) { return; }
    stmtSlot = &s; s->accept(*this);
  }

  void visit(const ast::IfStmt&) override {
    auto* ifs = static_cast<IfStmt*>(stmtSlot->get());
    pruneBlock(ifs->thenBody);
    pruneBlock(ifs->elseBody);
  }

  // No-ops for other nodes
  void visit(const ast::Module&) override {}
  void visit(const ast::FunctionDef&) override {}
  void visit(const ast::ReturnStmt&) override {}
  void visit(const ast::AssignStmt&) override {}
  void visit(const ast::ExprStmt&) override {}
  void visit(const ast::IntLiteral&) override {}
  void visit(const ast::BoolLiteral&) override {}
  void visit(const ast::FloatLiteral&) override {}
  void visit(const ast::StringLiteral&) override {}
  void visit(const ast::NoneLiteral&) override {}
  void visit(const ast::Name&) override {}
  void visit(const ast::Call&) override {}
  void visit(const ast::Binary&) override {}
  void visit(const ast::Unary&) override {}
  void visit(const ast::TupleLiteral&) override {}
  void visit(const ast::ListLiteral&) override {}
  void visit(const ast::ObjectLiteral&) override {}

  // Block pruning driver
  void pruneBlock(std::vector<std::unique_ptr<Stmt>>& body) {
    std::vector<std::unique_ptr<Stmt>> newBody;
    bool seenReturn = false;
    for (auto& st : body) {
      if (seenReturn) { ++removed; continue; }
      if (st->kind == NodeKind::ReturnStmt) {
        seenReturn = true;
        newBody.emplace_back(std::move(st));
        continue;
      }
      if (st->kind == NodeKind::IfStmt) { touch(st); }
      newBody.emplace_back(std::move(st));
    }
    body = std::move(newBody);
  }
};
} // namespace

size_t DCE::run(Module& module) {
  stats_.clear();
  DceVisitor vis;
  for (auto& func : module.functions) { vis.pruneBlock(func->body); }
  stats_["removed"] = vis.removed;
  return vis.removed;
}

} // namespace pycc::opt
