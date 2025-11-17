#pragma once

#include <string>
#include "ast/NodeKind.h"

namespace pycc::ast {

// Forward declarations to break include cycles
template <typename T, NodeKind K> struct Literal;
struct Name; struct Call; struct Binary; struct Unary; struct TupleLiteral; struct ListLiteral; struct DictLiteral; struct SetLiteral; struct ObjectLiteral; struct NoneLiteral; struct Attribute; struct Subscript; struct EllipsisLiteral; struct Compare; struct FStringLiteral; template <typename T, NodeKind K> struct Literal;
struct ReturnStmt; struct AssignStmt; struct AugAssignStmt; struct RaiseStmt; struct GlobalStmt; struct NonlocalStmt; struct AssertStmt; struct IfStmt; struct ExprStmt;
struct WhileStmt; struct ForStmt; struct BreakStmt; struct ContinueStmt; struct PassStmt;
struct TryStmt; struct ExceptHandler; struct WithItem; struct WithStmt;
struct Import; struct ImportFrom; struct Alias; struct ClassDef; struct DelStmt; struct DefStmt; struct NamedExpr; struct IfExpr; struct LambdaExpr; struct YieldExpr; struct AwaitExpr; struct ListComp; struct SetComp; struct DictComp; struct GeneratorExpr;
struct FunctionDef; struct Module;
struct MatchStmt; struct MatchCase;
struct PatternWildcard; struct PatternName; struct PatternLiteral; struct PatternOr; struct PatternAs; struct PatternClass; struct PatternSequence; struct PatternMapping;

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
  // Default no-ops for newly added nodes
  virtual void visit(const WhileStmt&) {}
  virtual void visit(const ForStmt&) {}
  virtual void visit(const BreakStmt&) {}
  virtual void visit(const ContinueStmt&) {}
  virtual void visit(const PassStmt&) {}
  virtual void visit(const TryStmt&) {}
  virtual void visit(const ExceptHandler&) {}
  virtual void visit(const WithItem&) {}
  virtual void visit(const WithStmt&) {}
  virtual void visit(const Import&) {}
  virtual void visit(const ImportFrom&) {}
  virtual void visit(const Alias&) {}
  virtual void visit(const ClassDef&) {}
  virtual void visit(const DelStmt&) {}
  virtual void visit(const DefStmt&) {}
  virtual void visit(const Literal<long long, NodeKind::IntLiteral>&) = 0;
  virtual void visit(const Literal<bool, NodeKind::BoolLiteral>&) = 0;
  virtual void visit(const Literal<double, NodeKind::FloatLiteral>&) = 0;
  virtual void visit(const Literal<std::string, NodeKind::StringLiteral>&) = 0;
  virtual void visit(const Literal<std::string, NodeKind::BytesLiteral>&) {}
  virtual void visit(const Literal<double, NodeKind::ImagLiteral>&) {}
  virtual void visit(const NoneLiteral&) = 0;
  virtual void visit(const EllipsisLiteral&) {}
  virtual void visit(const DictLiteral&) {}
  virtual void visit(const SetLiteral&) {}
  virtual void visit(const Name&) = 0;
  virtual void visit(const Attribute&) {}
  virtual void visit(const Subscript&) {}
  virtual void visit(const Call&) = 0;
  virtual void visit(const Binary&) = 0;
  virtual void visit(const Unary&) = 0;
  virtual void visit(const TupleLiteral&) = 0;
  virtual void visit(const ListLiteral&) = 0;
  virtual void visit(const ObjectLiteral&) = 0;
  virtual void visit(const NamedExpr&) {}
  virtual void visit(const IfExpr&) {}
  virtual void visit(const LambdaExpr&) {}
  virtual void visit(const Compare&) {}
  virtual void visit(const FStringLiteral&) {}
  virtual void visit(const YieldExpr&) {}
  virtual void visit(const AwaitExpr&) {}
  virtual void visit(const ListComp&) {}
  virtual void visit(const SetComp&) {}
  virtual void visit(const DictComp&) {}
  virtual void visit(const GeneratorExpr&) {}
  // match/case (shape-only; default no-ops)
  virtual void visit(const MatchStmt&) {}
  virtual void visit(const MatchCase&) {}
  virtual void visit(const PatternWildcard&) {}
  virtual void visit(const PatternName&) {}
  virtual void visit(const PatternLiteral&) {}
  virtual void visit(const PatternOr&) {}
  virtual void visit(const PatternAs&) {}
  virtual void visit(const PatternClass&) {}
  virtual void visit(const PatternSequence&) {}
  virtual void visit(const PatternMapping&) {}
  // control statements (no-op by default)
  virtual void visit(const AugAssignStmt&) {}
  virtual void visit(const RaiseStmt&) {}
  virtual void visit(const GlobalStmt&) {}
  virtual void visit(const NonlocalStmt&) {}
  virtual void visit(const AssertStmt&) {}
};

} // namespace pycc::ast
