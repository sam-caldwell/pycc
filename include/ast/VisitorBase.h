#pragma once

#include <string>
#include "ast/NodeKind.h"

namespace pycc::ast {

// Forward declarations to break include cycles
template <typename T, NodeKind K> struct Literal;
struct Name; struct Call; struct Binary; struct Unary; struct TupleLiteral; struct ListLiteral; struct NoneLiteral;
struct ReturnStmt; struct AssignStmt; struct IfStmt; struct ExprStmt;
struct FunctionDef; struct Module;

// Virtual visitor interface for AST traversal using polymorphism.
struct VisitorBase {
  virtual ~VisitorBase() = default;
  // One visit overload per concrete node type
  virtual void visit(const Module&) = 0;
  virtual void visit(const FunctionDef&) = 0;
  virtual void visit(const ReturnStmt&) = 0;
  virtual void visit(const AssignStmt&) = 0;
  virtual void visit(const IfStmt&) = 0;
  virtual void visit(const ExprStmt&) = 0;
  virtual void visit(const Literal<long long, NodeKind::IntLiteral>&) = 0;
  virtual void visit(const Literal<bool, NodeKind::BoolLiteral>&) = 0;
  virtual void visit(const Literal<double, NodeKind::FloatLiteral>&) = 0;
  virtual void visit(const Literal<std::string, NodeKind::StringLiteral>&) = 0;
  virtual void visit(const NoneLiteral&) = 0;
  virtual void visit(const Name&) = 0;
  virtual void visit(const Call&) = 0;
  virtual void visit(const Binary&) = 0;
  virtual void visit(const Unary&) = 0;
  virtual void visit(const TupleLiteral&) = 0;
  virtual void visit(const ListLiteral&) = 0;
};

} // namespace pycc::ast
