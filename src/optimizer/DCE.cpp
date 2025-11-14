/***
 * Name: pycc::opt::DCE (impl)
 */
#include "optimizer/DCE.h"
#include "ast/VisitorBase.h"

namespace pycc::opt {
using namespace pycc::ast;

namespace {
struct DceVisitor : public ast::VisitorBase {
  size_t removed{0};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  void touch(std::unique_ptr<Stmt>& s) {
    if (!s) { return; }
    stmtSlot = &s; s->accept(*this);
  }

  void visit(const IfStmt&) override {
    auto* ifs = static_cast<IfStmt*>(stmtSlot->get());
    pruneBlock(ifs->thenBody);
    pruneBlock(ifs->elseBody);
  }

  // No-ops for other nodes
  void visit(const Module&) override {}
  void visit(const FunctionDef&) override {}
  void visit(const ReturnStmt&) override {}
  void visit(const AssignStmt&) override {}
  void visit(const ExprStmt&) override {}
  void visit(const IntLiteral&) override {}
  void visit(const BoolLiteral&) override {}
  void visit(const FloatLiteral&) override {}
  void visit(const StringLiteral&) override {}
  void visit(const NoneLiteral&) override {}
  void visit(const Name&) override {}
  void visit(const Call&) override {}
  void visit(const Binary&) override {}
  void visit(const Unary&) override {}
  void visit(const TupleLiteral&) override {}
  void visit(const ListLiteral&) override {}
  void visit(const ObjectLiteral&) override {}

  // Block pruning driver
  // NOLINTNEXTLINE(readability-function-size)
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
