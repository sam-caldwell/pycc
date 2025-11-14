/***
 * Name: pycc::parse::Parser
 * Purpose: Build a minimal AST from tokens (Milestone 1 subset).
 * Inputs:
 *   - Token stream from Lexer (pull-based)
 * Outputs:
 *   - Module AST with function definitions and return int literals.
 * Theory of Operation:
 *   Uses a tiny recursive-descent parser reading from ITokenStream. It
 *   recognizes:
 *     module := { funcdef }
 *     funcdef := 'def' IDENT '(' [params] ')' '->' TYPE ':' NEWLINE INDENT {stmt} DEDENT
 *     stmt := return | if | assign | expr-stmt (ignored)
 */
#pragma once

#include "ast/Nodes.h"
#include "lexer/Lexer.h"
#include <memory>
#include <vector>

namespace pycc::parse {

class Parser {
 public:
  explicit Parser(lex::ITokenStream& stream) : ts_(stream) {}
  std::unique_ptr<ast::Module> parseModule();

 private:
  lex::ITokenStream& ts_;

  const lex::Token& peek() const;
  const lex::Token& peekNext() const;
  lex::Token get();
  bool match(lex::TokenKind tokenKind);
  void expect(lex::TokenKind tokenKind, const char* msg);

  std::unique_ptr<ast::FunctionDef> parseFunction();
  void parseParamList(std::vector<ast::Param>& outParams);
  std::unique_ptr<ast::Stmt> parseStatement();
  std::unique_ptr<ast::Expr> parseExpr();
  std::unique_ptr<ast::Expr> parseComparison();
  std::unique_ptr<ast::Expr> parseLogicalAnd();
  std::unique_ptr<ast::Expr> parseLogicalOr();
  std::unique_ptr<ast::Expr> parseAdditive();
  std::unique_ptr<ast::Expr> parseMultiplicative();
  std::unique_ptr<ast::Expr> parseUnary();
  std::unique_ptr<ast::Expr> parsePrimary();
  std::unique_ptr<ast::Expr> parsePostfix(std::unique_ptr<ast::Expr> base);
  static ast::TypeKind toTypeKind(const std::string& typeName);
};

} // namespace pycc::parse
