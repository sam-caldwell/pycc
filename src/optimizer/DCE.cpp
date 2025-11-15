/***
 * Name: pycc::opt::DCE (impl)
 */
#include "optimizer/DCE.h"
#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NodeKind.h"
#include "ast/NoneLiteral.h"
#include "ast/ObjectLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/Stmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Unary.h"
#include "ast/VisitorBase.h"
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace pycc::opt {
using pycc::ast::Module;
using pycc::ast::Stmt;
using pycc::ast::IfStmt;
using pycc::ast::NodeKind;

namespace {
struct DceVisitor : public ast::VisitorBase {
  size_t removed{0};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  void touch(std::unique_ptr<Stmt>& stmt) {
    if (!stmt) { return; }
    stmtSlot = &stmt; stmt->accept(*this);
  }

  void visit(const ast::IfStmt& ifStmt) override {
    (void)ifStmt;
    auto* ifs = static_cast<IfStmt*>(stmtSlot->get());
    pruneBlock(ifs->thenBody);
    pruneBlock(ifs->elseBody);
  }

  // No-ops for other nodes
  void visit(const ast::Module& module) override { (void)module; }
  void visit(const ast::FunctionDef& functionDef) override { (void)functionDef; }
  void visit(const ast::ReturnStmt& ret) override { (void)ret; }
  void visit(const ast::AssignStmt& assign) override { (void)assign; }
  void visit(const ast::ExprStmt& exprStmt) override { (void)exprStmt; }
  void visit(const ast::IntLiteral& intLiteral) override { (void)intLiteral; }
  void visit(const ast::BoolLiteral& boolLiteral) override { (void)boolLiteral; }
  void visit(const ast::FloatLiteral& floatLiteral) override { (void)floatLiteral; }
  void visit(const ast::StringLiteral& stringLiteral) override { (void)stringLiteral; }
  void visit(const ast::NoneLiteral& noneLiteral) override { (void)noneLiteral; }
  void visit(const ast::Name& name) override { (void)name; }
  void visit(const ast::Call& call) override { (void)call; }
  void visit(const ast::Binary& binary) override { (void)binary; }
  void visit(const ast::Unary& unary) override { (void)unary; }
  void visit(const ast::TupleLiteral& tupleLiteral) override { (void)tupleLiteral; }
  void visit(const ast::ListLiteral& listLiteral) override { (void)listLiteral; }
  void visit(const ast::ObjectLiteral& objectLiteral) override { (void)objectLiteral; }

  // Block pruning driver
  void pruneBlock(std::vector<std::unique_ptr<Stmt>>& body) {
    std::vector<std::unique_ptr<Stmt>> newBody;
    bool seenReturn = false;
    for (auto& stmt : body) {
      if (seenReturn) { ++removed; continue; }
      if (stmt->kind == NodeKind::ReturnStmt) {
        seenReturn = true;
        newBody.emplace_back(std::move(stmt));
        continue;
      }
      if (stmt->kind == NodeKind::IfStmt) { touch(stmt); }
      newBody.emplace_back(std::move(stmt));
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
