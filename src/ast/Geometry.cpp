/***
 * Name: pycc::ast::ComputeGeometry
 * Purpose: Traverse AST and compute node count and max depth.
 */
#include "ast/GeometrySummary.h"
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
#include "ast/NoneLiteral.h"
#include "ast/ObjectLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Unary.h"
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
    for (const auto& func : module.functions) { const DepthScope scope{depth}; func->accept(*this); }
  }
  void visit(const FunctionDef& func) override {
    bump();
    for (const auto& stmt : func.body) { const DepthScope scope{depth}; stmt->accept(*this); }
  }
  void visit(const ReturnStmt& ret) override {
    bump();
    const DepthScope scope{depth};
    if (ret.value) { ret.value->accept(*this); }
  }
  void visit(const AssignStmt& asg) override {
    bump();
    const DepthScope scope{depth};
    if (asg.value) { asg.value->accept(*this); }
  }
  void visit(const ExprStmt& expr) override {
    bump();
    const DepthScope scope{depth};
    if (expr.value) { expr.value->accept(*this); }
  }
  void visit(const IfStmt& iff) override {
    bump();
    {
      const DepthScope scope{depth};
      if (iff.cond) { iff.cond->accept(*this); }
    }
    for (const auto& stmtThen : iff.thenBody) { const DepthScope scope{depth}; stmtThen->accept(*this); }
    for (const auto& stmtElse : iff.elseBody) { const DepthScope scope{depth}; stmtElse->accept(*this); }
  }
  void visit(const IntLiteral& intLiteral) override { (void)intLiteral; bump(); }
  void visit(const BoolLiteral& boolLiteral) override { (void)boolLiteral; bump(); }
  void visit(const FloatLiteral& floatLiteral) override { (void)floatLiteral; bump(); }
  void visit(const StringLiteral& stringLiteral) override { (void)stringLiteral; bump(); }
  void visit(const NoneLiteral& noneLiteral) override { (void)noneLiteral; bump(); }
  void visit(const Unary& unary) override {
    bump();
    const DepthScope scope{depth};
    if (unary.operand) { unary.operand->accept(*this); }
  }
  void visit(const TupleLiteral& tuple) override {
    bump();
    for (const auto& elem : tuple.elements) { const DepthScope scope{depth}; elem->accept(*this); }
  }
  void visit(const ListLiteral& list) override {
    bump();
    for (const auto& elem : list.elements) { const DepthScope scope{depth}; elem->accept(*this); }
  }
  void visit(const ObjectLiteral& obj) override {
    bump();
    for (const auto& field : obj.fields) { const DepthScope scope{depth}; field->accept(*this); }
  }
  void visit(const Name& name) override { (void)name; bump(); }
  void visit(const Call& call) override {
    bump();
    {
      const DepthScope scope{depth};
      if (call.callee) { call.callee->accept(*this); }
    }
    for (const auto& arg : call.args) { const DepthScope scope{depth}; arg->accept(*this); }
  }
  void visit(const Binary& bin) override {
    bump();
    { const DepthScope scope{depth}; if (bin.lhs) { bin.lhs->accept(*this); } }
    { const DepthScope scope{depth}; if (bin.rhs) { bin.rhs->accept(*this); } }
  }
};

GeometrySummary ComputeGeometry(const Module& module) {
  GeometryVisitor visitor;
  module.accept(visitor);
  return GeometrySummary{visitor.nodes, visitor.maxDepth};
}

} // namespace pycc::ast
