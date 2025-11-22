/**
 * @file
 * @brief AST geometry visitor declarations.
 */
#pragma once

#include <cstdint>
#include "ast/VisitorBase.h"

namespace pycc::ast {

// Forward-declared, defined in src/ast/GeometryVisitor.cpp
struct GeometryVisitor final : public VisitorBase {
  uint64_t nodes{0};
  uint64_t maxDepth{0};
  uint64_t depth{0};

  void bump();

  struct DepthScope {
    uint64_t& d;
    explicit DepthScope(uint64_t& ref);
    ~DepthScope();
    DepthScope(const DepthScope&) = delete;
    DepthScope& operator=(const DepthScope&) = delete;
    DepthScope(DepthScope&&) = delete;
    DepthScope& operator=(DepthScope&&) = delete;
  };

  void visit(const Module& module) override;
  void visit(const FunctionDef& func) override;
  void visit(const ReturnStmt& ret) override;
  void visit(const AssignStmt& asg) override;
  void visit(const ExprStmt& expr) override;
  void visit(const IfStmt& iff) override;

  void visit(const Literal<long long, NodeKind::IntLiteral>&) override;
  void visit(const Literal<bool, NodeKind::BoolLiteral>&) override;
  void visit(const Literal<double, NodeKind::FloatLiteral>&) override;
  void visit(const Literal<std::string, NodeKind::StringLiteral>&) override;
  void visit(const NoneLiteral& noneLiteral) override;
  void visit(const Unary& unary) override;
  void visit(const TupleLiteral& tuple) override;
  void visit(const ListLiteral& list) override;
  void visit(const ObjectLiteral& obj) override;
  void visit(const Name& name) override;
  void visit(const Call& call) override;
  void visit(const Binary& bin) override;
};

} // namespace pycc::ast
