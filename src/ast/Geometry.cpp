/***
 * Name: pycc::ast::ComputeGeometry
 * Purpose: Traverse AST and compute node count and max depth.
 */
#include "ast/GeometrySummary.h"
#include "ast/Nodes.h"
#include "ast/VisitorBase.h"
#include <algorithm>
#include <cstdint>

namespace pycc::ast {

struct GeometryVisitor : public VisitorBase {
  uint64_t nodes{0};
  uint64_t maxDepth{0};
  uint64_t depth{0};

  void bump() { ++nodes; maxDepth = std::max(maxDepth, depth); }

  struct DepthScope {
    uint64_t& d;
    explicit DepthScope(uint64_t& ref) : d(ref) { ++d; }
    ~DepthScope() { --d; }
    DepthScope(const DepthScope&) = delete;
    DepthScope& operator=(const DepthScope&) = delete;
    DepthScope(DepthScope&&) = delete;
    DepthScope& operator=(DepthScope&&) = delete;
  };

  void visit(const Module& module) override {
    bump();
    for (const auto& func : module.functions) { DepthScope _{depth}; func->accept(*this); }
  }
  void visit(const FunctionDef& func) override {
    bump();
    for (const auto& stmt : func.body) { DepthScope _{depth}; stmt->accept(*this); }
  }
  void visit(const ReturnStmt& ret) override {
    bump();
    DepthScope _{depth};
    if (ret.value) { ret.value->accept(*this); }
  }
  void visit(const AssignStmt& asg) override {
    bump();
    DepthScope _{depth};
    if (asg.value) { asg.value->accept(*this); }
  }
  void visit(const ExprStmt& expr) override {
    bump();
    DepthScope _{depth};
    if (expr.value) { expr.value->accept(*this); }
  }
  void visit(const IfStmt& iff) override {
    bump();
    {
      DepthScope _{depth};
      if (iff.cond) { iff.cond->accept(*this); }
    }
    for (const auto& stmtThen : iff.thenBody) { DepthScope _{depth}; stmtThen->accept(*this); }
    for (const auto& stmtElse : iff.elseBody) { DepthScope _{depth}; stmtElse->accept(*this); }
  }
  void visit(const IntLiteral&) override { bump(); }
  void visit(const BoolLiteral&) override { bump(); }
  void visit(const FloatLiteral&) override { bump(); }
  void visit(const StringLiteral&) override { bump(); }
  void visit(const NoneLiteral&) override { bump(); }
  void visit(const Unary& unary) override {
    bump();
    DepthScope _{depth};
    if (unary.operand) { unary.operand->accept(*this); }
  }
  void visit(const TupleLiteral& tuple) override {
    bump();
    for (const auto& elem : tuple.elements) { DepthScope _{depth}; elem->accept(*this); }
  }
  void visit(const ListLiteral& list) override {
    bump();
    for (const auto& elem : list.elements) { DepthScope _{depth}; elem->accept(*this); }
  }
  void visit(const ObjectLiteral& obj) override {
    bump();
    for (const auto& field : obj.fields) { DepthScope _{depth}; field->accept(*this); }
  }
  void visit(const Name&) override { bump(); }
  void visit(const Call& call) override {
    bump();
    {
      DepthScope _{depth};
      if (call.callee) { call.callee->accept(*this); }
    }
    for (const auto& arg : call.args) { DepthScope _{depth}; arg->accept(*this); }
  }
  void visit(const Binary& bin) override {
    bump();
    { DepthScope _{depth}; if (bin.lhs) { bin.lhs->accept(*this); } }
    { DepthScope _{depth}; if (bin.rhs) { bin.rhs->accept(*this); } }
  }
};

GeometrySummary ComputeGeometry(const Module& module) {
  GeometryVisitor visitor;
  module.accept(visitor);
  return GeometrySummary{visitor.nodes, visitor.maxDepth};
}

} // namespace pycc::ast
