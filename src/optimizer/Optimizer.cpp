/***
 * Name: pycc::opt::Optimizer (impl)
 * Purpose: Visitor-based traversal for future optimization passes.
 */
#include "optimizer/Optimizer.h"
#include "ast/VisitorBase.h"

namespace pycc::opt {

struct Counter : public ast::VisitorBase {
  Stats s;
  void visit(const ast::Module& module) override {
    ++s.nodesVisited;
    for (const auto& func : module.functions) { func->accept(*this); }
  }
  void visit(const ast::FunctionDef& functionDef) override {
    ++s.nodesVisited;
    for (const auto& stmt : functionDef.body) { stmt->accept(*this); }
  }
  void visit(const ast::ReturnStmt& ret) override {
    ++s.nodesVisited;
    ++s.stmtsVisited;
    if (ret.value) { ret.value->accept(*this); }
  }
  void visit(const ast::AssignStmt& assign) override {
    ++s.nodesVisited;
    ++s.stmtsVisited;
    if (assign.value) { assign.value->accept(*this); }
  }
  void visit(const ast::IfStmt& ifStmt) override {
    ++s.nodesVisited;
    ++s.stmtsVisited;
    if (ifStmt.cond) { ifStmt.cond->accept(*this); }
    for (const auto& thenStmt : ifStmt.thenBody) { thenStmt->accept(*this); }
    for (const auto& elseStmt : ifStmt.elseBody) { elseStmt->accept(*this); }
  }
  void visit(const ast::ExprStmt& exprStmt) override {
    ++s.nodesVisited;
    ++s.stmtsVisited;
    if (exprStmt.value) { exprStmt.value->accept(*this); }
  }
  void visit([[maybe_unused]] const ast::IntLiteral& intLiteral) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit([[maybe_unused]] const ast::BoolLiteral& boolLiteral) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit([[maybe_unused]] const ast::FloatLiteral& floatLiteral) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit([[maybe_unused]] const ast::StringLiteral& stringLiteral) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit([[maybe_unused]] const ast::NoneLiteral& noneLiteral) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit([[maybe_unused]] const ast::Name& name) override { ++s.nodesVisited; ++s.exprsVisited; }
  void visit(const ast::Call& callNode) override {
    ++s.nodesVisited;
    ++s.exprsVisited;
    if (callNode.callee) { callNode.callee->accept(*this); }
    for (const auto& arg : callNode.args) { arg->accept(*this); }
  }
  void visit(const ast::Binary& binaryNode) override {
    ++s.nodesVisited;
    ++s.exprsVisited;
    if (binaryNode.lhs) { binaryNode.lhs->accept(*this); }
    if (binaryNode.rhs) { binaryNode.rhs->accept(*this); }
  }
  void visit(const ast::Unary& unaryNode) override {
    ++s.nodesVisited;
    ++s.exprsVisited;
    if (unaryNode.operand) { unaryNode.operand->accept(*this); }
  }
  void visit(const ast::TupleLiteral& tupleLiteral) override {
    ++s.nodesVisited;
    ++s.exprsVisited;
    for (const auto& element : tupleLiteral.elements) { element->accept(*this); }
  }
  void visit(const ast::ListLiteral& listLiteral) override {
    ++s.nodesVisited;
    ++s.exprsVisited;
    for (const auto& element : listLiteral.elements) { element->accept(*this); }
  }
  void visit(const ast::ObjectLiteral& obj) override {
    ++s.nodesVisited;
    ++s.exprsVisited;
    for (const auto& v : obj.fields) { v->accept(*this); }
  }
};
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Stats Optimizer::analyze(const ast::Module& module) const {
  Counter counter; // NOLINT(misc-const-correctness)
  module.accept(counter);
  return counter.s;
}

} // namespace pycc::opt
