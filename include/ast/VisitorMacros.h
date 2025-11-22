/**
 * @file
 * @brief Helper macros for AST visitor wiring.
 */
#pragma once

// Simple visitor helper macros to reduce boilerplate

#define VISITOR_DECLARE() \
  void visit(const pycc::ast::Module&); \
  void visit(const pycc::ast::FunctionDef&); \
  void visit(const pycc::ast::ReturnStmt&); \
  void visit(const pycc::ast::AssignStmt&); \
  void visit(const pycc::ast::ExprStmt&); \
  void visit(const pycc::ast::IfStmt&); \
  void visit(const pycc::ast::IntLiteral&); \
  void visit(const pycc::ast::BoolLiteral&); \
  void visit(const pycc::ast::FloatLiteral&); \
  void visit(const pycc::ast::Name&); \
  void visit(const pycc::ast::Call&); \
  void visit(const pycc::ast::Binary&); \
  void visit(const pycc::ast::Unary&); \
  void visit(const pycc::ast::TupleLiteral&); \
  void visit(const pycc::ast::ListLiteral&)
