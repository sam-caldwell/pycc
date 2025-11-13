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
#include "ast/ObjectLiteral.h"
#include "ast/Name.h"
#include "ast/NoneLiteral.h"
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

  void bump() {
    ++nodes;
    maxDepth = std::max(maxDepth, depth);
  }

  void visit(const Module& module) override {
    bump();
    for (const auto& func : module.functions) {
      ++depth;
      func->accept(*this);
      --depth;
    }
  }
  void visit(const FunctionDef& func) override {
    bump();
    for (const auto& stmt : func.body) {
      ++depth;
      stmt->accept(*this);
      --depth;
    }
  }
  void visit(const ReturnStmt& ret) override {
    bump();
    ++depth;
    if (ret.value) { ret.value->accept(*this); }
    --depth;
  }
  void visit(const AssignStmt& asg) override {
    bump();
    ++depth;
    if (asg.value) { asg.value->accept(*this); }
    --depth;
  }
  void visit(const ExprStmt& expr) override {
    bump();
    ++depth;
    if (expr.value) { expr.value->accept(*this); }
    --depth;
  }
  void visit(const IfStmt& iff) override {
    bump();
    ++depth;
    if (iff.cond) { iff.cond->accept(*this); }
    --depth;
    for (const auto& stmtThen : iff.thenBody) {
      ++depth; stmtThen->accept(*this); --depth;
    }
    for (const auto& stmtElse : iff.elseBody) {
      ++depth; stmtElse->accept(*this); --depth;
    }
  }
  void visit(const IntLiteral& /*unused*/) override { bump(); }
  void visit(const BoolLiteral& /*unused*/) override { bump(); }
  void visit(const FloatLiteral& /*unused*/) override { bump(); }
  void visit(const StringLiteral& /*unused*/) override { bump(); }
  void visit(const NoneLiteral& /*unused*/) override { bump(); }
  void visit(const Unary& unary) override {
    bump();
    ++depth;
    if (unary.operand) { unary.operand->accept(*this); }
    --depth;
  }
  void visit(const TupleLiteral& tuple) override {
    bump();
    for (const auto& elem : tuple.elements) { ++depth; elem->accept(*this); --depth; }
  }
  void visit(const ListLiteral& list) override {
    bump();
    for (const auto& elem : list.elements) { ++depth; elem->accept(*this); --depth; }
  }
  void visit(const ObjectLiteral& obj) override {
    bump();
    for (const auto& field : obj.fields) { ++depth; field->accept(*this); --depth; }
  }
  void visit(const Name& /*unused*/) override { bump(); }
  void visit(const Call& call) override {
    bump();
    ++depth;
    if (call.callee) { call.callee->accept(*this); }
    --depth;
    for (const auto& arg : call.args) { ++depth; arg->accept(*this); --depth; }
  }
  void visit(const Binary& bin) override {
    bump();
    ++depth;
    if (bin.lhs) { bin.lhs->accept(*this); }
    --depth;
    ++depth;
    if (bin.rhs) { bin.rhs->accept(*this); }
    --depth;
  }
};

GeometrySummary ComputeGeometry(const Module& module) {
  GeometryVisitor visitor;
  module.accept(visitor);
  return GeometrySummary{visitor.nodes, visitor.maxDepth};
}

} // namespace pycc::ast
