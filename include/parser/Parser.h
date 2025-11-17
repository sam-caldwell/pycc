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
  std::unique_ptr<ast::ClassDef> parseClass();
  void parseParamList(std::vector<ast::Param>& outParams);
  void parseOptionalParamType(ast::Param& param);
  void parseSuiteInto(std::vector<std::unique_ptr<ast::Stmt>>& out);
  std::unique_ptr<ast::Stmt> parseIfStmt();
  std::unique_ptr<ast::Stmt> parseWhileStmt();
  std::unique_ptr<ast::Stmt> parseForStmt();
  std::unique_ptr<ast::Stmt> parseTryStmt();
  std::unique_ptr<ast::Stmt> parseWithStmt();
  std::unique_ptr<ast::Stmt> parseImportStmt();
  std::unique_ptr<ast::Stmt> parseStatement();
  std::unique_ptr<ast::Expr> parseExpr();
  std::unique_ptr<ast::Expr> parseComparison();
  std::unique_ptr<ast::Expr> parseBitwiseOr();
  std::unique_ptr<ast::Expr> parseBitwiseXor();
  std::unique_ptr<ast::Expr> parseBitwiseAnd();
  std::unique_ptr<ast::Expr> parseShift();
  std::unique_ptr<ast::Expr> parseLogicalAnd();
  std::unique_ptr<ast::Expr> parseLogicalOr();
  std::unique_ptr<ast::Expr> parseAdditive();
  std::unique_ptr<ast::Expr> parseMultiplicative();
  std::unique_ptr<ast::Expr> parseUnary();
  std::unique_ptr<ast::Expr> parsePrimary();
  std::unique_ptr<ast::Expr> parsePostfix(std::unique_ptr<ast::Expr> base);
  std::unique_ptr<ast::Expr> parseAtom();
  static ast::TypeKind toTypeKind(const std::string& typeName);

  // Refactoring helpers (kept private)
  static std::string unquoteString(std::string text);
  std::unique_ptr<ast::Expr> parseExprFromString(const std::string& text,
                                                const std::string& name);
  std::unique_ptr<ast::Expr> parseListLiteral(const lex::Token& openTok);
  std::unique_ptr<ast::Expr> parseTupleOrParen(const lex::Token& openTok);
  static std::unique_ptr<ast::Expr> parseNameOrNone(const lex::Token& tok);
  struct ArgList {
    std::vector<std::unique_ptr<ast::Expr>> positional;
    std::vector<ast::KeywordArg> keywords;
    std::vector<std::unique_ptr<ast::Expr>> starArgs;
    std::vector<std::unique_ptr<ast::Expr>> kwStarArgs;
  };
  ArgList parseArgList();
  static bool desugarObjectCall(std::unique_ptr<ast::Expr>& base,
                                std::vector<std::unique_ptr<ast::Expr>>& args);
  static ast::BinaryOperator mulOpFor(lex::TokenKind kind);

  // Set ExprContext recursively on assignment/del targets
  static void setTargetContext(ast::Expr* e, ast::ExprContext ctx);

  // Parse zero or more decorators: '@' expr [NEWLINE]
  std::vector<std::unique_ptr<ast::Expr>> parseDecorators();

  // Validate whether an expression is a legal assignment target
  static bool isValidAssignmentTarget(const ast::Expr* e);

  // Look ahead on the current logical line for an un-nested '='
  bool hasPendingEqualOnLine();
  bool hasPendingAugAssignOnLine(lex::TokenKind& which);

  // match/case
  std::unique_ptr<ast::Stmt> parseMatchStmt();
  std::unique_ptr<ast::MatchCase> parseMatchCase();
  std::unique_ptr<ast::Pattern> parsePattern();
  std::unique_ptr<ast::Pattern> parsePatternOr();
  std::unique_ptr<ast::Pattern> parseSimplePattern();

  // New literals and statements
  std::unique_ptr<ast::Expr> parseDictOrSetLiteral(const lex::Token& openTok);
  std::unique_ptr<ast::Stmt> parseRaiseStmt();
  std::unique_ptr<ast::Stmt> parseGlobalStmt();
  std::unique_ptr<ast::Stmt> parseNonlocalStmt();
  std::unique_ptr<ast::Stmt> parseAssertStmt();

  // Comprehension helpers
  std::vector<ast::ComprehensionFor> parseComprehensionFors();
};

} // namespace pycc::parse
