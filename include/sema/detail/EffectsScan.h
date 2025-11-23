/***
 * @file
 * @brief EffectsScan: lightweight visitor to detect may-raise expressions.
 */
#pragma once

#include "ast/VisitorBase.h"

namespace pycc::sema {

/***
 * @brief Visitor that marks mayRaise when encountering operations likely to raise.
 */
struct EffectsScan : public ast::VisitorBase {
  bool mayRaise{false};
  void visit(const ast::Module&) override;
  void visit(const ast::FunctionDef&) override;
  void visit(const ast::ReturnStmt& r) override;
  void visit(const ast::AssignStmt& as) override;
  void visit(const ast::IfStmt& iff) override;
  void visit(const ast::ExprStmt& es) override;
  void visit(const ast::Call& c) override;
  void visit(const ast::Attribute& a) override;
  void visit(const ast::Subscript& s) override;
  void visit(const ast::Binary& b) override;
  void visit(const ast::Unary& u) override;
  void visit(const ast::TupleLiteral& t) override;
  void visit(const ast::ListLiteral& l) override;
  void visit(const ast::ObjectLiteral&) override;
  void visit(const ast::Name&) override;
  void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>&) override;
  void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>&) override;
  void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>&) override;
  void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>&) override;
  void visit(const ast::NoneLiteral&) override;
};

} // namespace pycc::sema
