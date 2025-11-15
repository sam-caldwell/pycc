/***
 * Name: pycc::parse::Parser (impl)
 * Purpose: Minimal parser for Milestone 1.
 */
#include "parser/Parser.h"
#include <cstddef>
#include <stdexcept>

namespace pycc::parse {

using TK = lex::TokenKind;

const lex::Token& Parser::peek() const { return ts_.peek(0); }
const lex::Token& Parser::peekNext() const { return ts_.peek(1); }
lex::Token Parser::get() { return ts_.next(); }

bool Parser::match(TK tokenKind) {
  if (peek().kind == tokenKind) { (void)get(); return true; }
  return false;
}

void Parser::expect(TK tokenKind, const char* msg) {
  if (!match(tokenKind)) { throw std::runtime_error(std::string("Parse error: expected ") + msg); }
}

std::unique_ptr<ast::Module> Parser::parseModule() {
  auto mod = std::make_unique<ast::Module>();
  while (peek().kind != TK::End) {
    if (peek().kind == TK::Newline) { get(); continue; }
    if (peek().kind == TK::Def) {
      mod->functions.emplace_back(parseFunction());
    } else {
      // Skip unknown top-level constructs
      get();
    }
  }
  return mod;
}

std::unique_ptr<ast::FunctionDef> Parser::parseFunction() {
  expect(TK::Def, "'def'");
  auto nameTok = get();
  if (nameTok.kind != TK::Ident) { throw std::runtime_error("Parse error: expected function name"); }
  expect(TK::LParen, "'('");
  std::vector<ast::Param> params;
  parseParamList(params);
  expect(TK::RParen, "')'");
  expect(TK::Arrow, "'->'");
  auto typeTok = get();
  if (typeTok.kind != TK::TypeIdent) { throw std::runtime_error("Parse error: expected return type ident"); }
  // Limited Optional/Union syntax support: allow T | None and ignore the None part for now
  if (peek().kind == TK::Pipe) {
    get(); // consume '|'
    const auto& typeTok2 = get();
    if (typeTok2.kind != TK::TypeIdent) { throw std::runtime_error("Parse error: expected type ident after '|'"); }
    // ignore t2
  }
  expect(TK::Colon, "':'");
  if (peek().kind == TK::Newline) { get(); }
  expect(TK::Indent, "indent");

  auto func = std::make_unique<ast::FunctionDef>(nameTok.text, toTypeKind(typeTok.text));
  func->params = std::move(params);
  while (peek().kind != TK::Dedent && peek().kind != TK::End) {
    if (peek().kind == TK::Newline) { get(); continue; }
    func->body.emplace_back(parseStatement());
  }
  expect(TK::Dedent, "dedent");
  return func;
}

std::unique_ptr<ast::Stmt> Parser::parseStatement() {
  if (peek().kind == TK::Return) {
    get();
    auto expr = parseExpr();
    // NEWLINE may follow (but parseFunction loop also handles)
    return std::make_unique<ast::ReturnStmt>(std::move(expr));
  }
  if (peek().kind == TK::If) { return parseIfStmt(); }
  if (peek().kind == TK::Ident && peekNext().kind == TK::Equal) {
    std::string name = get().text; // ident
    expect(TK::Equal, "'='");
    auto rhs = parseExpr();
    return std::make_unique<ast::AssignStmt>(name, std::move(rhs));
  }
  // Fallback: expression statement
  auto expr = parseExpr();
  return std::make_unique<ast::ExprStmt>(std::move(expr));
}

void Parser::parseParamList(std::vector<ast::Param>& outParams) {
  if (peek().kind == TK::RParen) { return; }
  for (;;) {
    const auto& pNameTok = get();
    if (pNameTok.kind != TK::Ident) { throw std::runtime_error("Parse error: expected parameter name"); }
    ast::Param param; param.name = pNameTok.text; param.type = ast::TypeKind::NoneType;
    parseOptionalParamType(param);
    outParams.push_back(std::move(param));
    if (peek().kind == TK::Comma) { get(); continue; }
    break;
  }
}

void Parser::parseOptionalParamType(ast::Param& param) {
  if (peek().kind != TK::Colon) { return; }
  get();
  const auto& pTypeTok = get();
  if (pTypeTok.kind != TK::TypeIdent) { throw std::runtime_error("Parse error: expected type ident after ':'"); }
  param.type = toTypeKind(pTypeTok.text);
}

std::unique_ptr<ast::Stmt> Parser::parseIfStmt() {
  get();
  auto cond = parseExpr();
  expect(TK::Colon, ":'");
  expect(TK::Newline, "newline");
  expect(TK::Indent, "indent");
  auto ifs = std::make_unique<ast::IfStmt>(std::move(cond));
  parseSuiteInto(ifs->thenBody);
  if (peek().kind == TK::Else) {
    get();
    expect(TK::Colon, ":'");
    expect(TK::Newline, "newline");
    expect(TK::Indent, "indent");
    parseSuiteInto(ifs->elseBody);
  }
  return ifs;
}

void Parser::parseSuiteInto(std::vector<std::unique_ptr<ast::Stmt>>& out) {
  while (peek().kind != TK::Dedent && peek().kind != TK::End) {
    if (peek().kind == TK::Newline) { get(); continue; }
    out.emplace_back(parseStatement());
  }
  expect(TK::Dedent, "dedent");
}

std::unique_ptr<ast::Expr> Parser::parseExpr() { return parseLogicalOr(); }

std::unique_ptr<ast::Expr> Parser::parseLogicalOr() {
  auto lhs = parseLogicalAnd();
  while (peek().kind == TK::Or) {
    auto tok = get();
    auto rhs = parseLogicalAnd();
    auto node = std::make_unique<ast::Binary>(ast::BinaryOperator::Or, std::move(lhs), std::move(rhs));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    lhs = std::move(node);
  }
  return lhs;
}

std::unique_ptr<ast::Expr> Parser::parseLogicalAnd() {
  auto lhs = parseComparison();
  while (peek().kind == TK::And) {
    auto tok = get();
    auto rhs = parseComparison();
    auto node = std::make_unique<ast::Binary>(ast::BinaryOperator::And, std::move(lhs), std::move(rhs));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    lhs = std::move(node);
  }
  return lhs;
}

std::unique_ptr<ast::Expr> Parser::parseComparison() {
  auto lhs = parseAdditive();
  using TK = lex::TokenKind;
  if (peek().kind == TK::EqEq || peek().kind == TK::NotEq ||
      peek().kind == TK::Lt || peek().kind == TK::Le ||
      peek().kind == TK::Gt || peek().kind == TK::Ge) {
    auto opTok = get();
    auto rhs = parseAdditive();
    ast::BinaryOperator binOp = ast::BinaryOperator::Eq;
    switch (opTok.kind) {
      case TK::EqEq: binOp = ast::BinaryOperator::Eq; break;
      case TK::NotEq: binOp = ast::BinaryOperator::Ne; break;
      case TK::Lt: binOp = ast::BinaryOperator::Lt; break;
      case TK::Le: binOp = ast::BinaryOperator::Le; break;
      case TK::Gt: binOp = ast::BinaryOperator::Gt; break;
      case TK::Ge: binOp = ast::BinaryOperator::Ge; break;
      default: break;
    }
    auto node = std::make_unique<ast::Binary>(binOp, std::move(lhs), std::move(rhs));
    node->line = opTok.line; node->col = opTok.col; node->file = opTok.file;
    return node;
  }
  return lhs;
}

std::unique_ptr<ast::Expr> Parser::parseAdditive() {
  auto lhs = parseMultiplicative();
  for (;;) {
    if (peek().kind == TK::Plus || peek().kind == TK::Minus) {
      auto opTok = get();
      auto rhs = parseMultiplicative();
      const ast::BinaryOperator binOp = (opTok.kind == TK::Plus) ? ast::BinaryOperator::Add : ast::BinaryOperator::Sub;
      auto node = std::make_unique<ast::Binary>(binOp, std::move(lhs), std::move(rhs));
      node->line = opTok.line; node->col = opTok.col; node->file = opTok.file;
      lhs = std::move(node);
      continue;
    }
    break;
  }
  return lhs;
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
std::unique_ptr<ast::Expr> Parser::parseMultiplicative() {
  // Allow postfix on unary (calls) before binding operators
  auto lhs = parsePostfix(parseUnary());
  for (;;) {
    if (peek().kind == TK::Star || peek().kind == TK::Slash || peek().kind == TK::Percent) {
      auto opTok = get();
      auto rhs = parsePostfix(parseUnary());
      ast::BinaryOperator binOp = ast::BinaryOperator::Mul;
      if (opTok.kind == TK::Slash) { binOp = ast::BinaryOperator::Div; }
      else if (opTok.kind == TK::Percent) { binOp = ast::BinaryOperator::Mod; }
      auto node = std::make_unique<ast::Binary>(binOp, std::move(lhs), std::move(rhs));
      node->line = opTok.line; node->col = opTok.col; node->file = opTok.file;
      lhs = std::move(node);
      continue;
    }
    break;
  }
  return lhs;
}

std::unique_ptr<ast::Expr> Parser::parseUnary() {
  if (peek().kind == TK::Minus) {
    auto minusTok = get(); // '-'
    auto operand = parsePostfix(parsePrimary());
    if (operand->kind == ast::NodeKind::IntLiteral) {
      auto* lit = static_cast<ast::IntLiteral*>(operand.get());
      return std::make_unique<ast::IntLiteral>(-lit->value);
    }
    auto node = std::make_unique<ast::Unary>(ast::UnaryOperator::Neg, std::move(operand));
    node->line = minusTok.line; node->col = minusTok.col; node->file = minusTok.file;
    return node;
  }
  if (peek().kind == TK::Not) {
    auto notTok = get();
    auto operand = parsePostfix(parsePrimary());
    auto node = std::make_unique<ast::Unary>(ast::UnaryOperator::Not, std::move(operand));
    node->line = notTok.line; node->col = notTok.col; node->file = notTok.file;
    return node;
  }
  if (peek().kind == TK::Plus) {
    get(); // unary plus: no-op
    return parsePostfix(parsePrimary());
  }
  return parsePrimary();
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
std::unique_ptr<ast::Expr> Parser::parsePrimary() {
  const auto& tok = get();
  if (tok.kind == TK::Int) {
    auto node = std::make_unique<ast::IntLiteral>(std::stoll(tok.text));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  if (tok.kind == TK::Float) {
    auto node = std::make_unique<ast::FloatLiteral>(std::stod(tok.text));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  if (tok.kind == TK::String) {
    std::string text = tok.text;
    if (!text.empty() && (text.front() == '"' || text.front() == '\'')) {
      // strip surrounding quotes
      if (text.size() >= 2 && text.back() == text.front()) { text = text.substr(1, text.size() - 2); }
    }
    auto node = std::make_unique<ast::StringLiteral>(text);
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  if (tok.kind == TK::Ident || tok.kind == TK::TypeIdent) {
    if (tok.kind == TK::TypeIdent && tok.text == "None") {
      auto node = std::make_unique<ast::NoneLiteral>();
      node->line = tok.line; node->col = tok.col; node->file = tok.file;
      return node;
    }
    auto node = std::make_unique<ast::Name>(tok.text);
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  if (tok.kind == TK::BoolLit) {
    bool boolVal = (tok.text == "True");
    auto node = std::make_unique<ast::BoolLiteral>(boolVal);
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  if (tok.kind == TK::LBracket) {
    auto list = std::make_unique<ast::ListLiteral>();
    if (peek().kind != TK::RBracket) {
      for (;;) {
        auto expr = parseExpr();
        list->elements.emplace_back(std::move(expr));
        if (peek().kind == TK::Comma) { get(); continue; }
        break;
      }
    }
    expect(TK::RBracket, "]'");
    list->line = tok.line; list->col = tok.col; list->file = tok.file;
    return list;
  }
  if (tok.kind == TK::LParen) {
    auto first = parseExpr();
    if (peek().kind == TK::Comma) {
      auto tup = std::make_unique<ast::TupleLiteral>();
      tup->elements.emplace_back(std::move(first));
      while (peek().kind == TK::Comma) {
        get();
        if (peek().kind == TK::RParen) { break; }
        auto expr2 = parseExpr();
        tup->elements.emplace_back(std::move(expr2));
      }
      expect(TK::RParen, "')'");
      tup->line = tok.line; tup->col = tok.col; tup->file = tok.file;
      return tup;
    }
    expect(TK::RParen, "')'");
    return first;
  }
  throw std::runtime_error("Parse error: expected primary expression");
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
std::unique_ptr<ast::Expr> Parser::parsePostfix(std::unique_ptr<ast::Expr> base) {
  // Handle calls and object(...) sugar
  for (;;) {
    if (peek().kind == TK::LParen) {
      get(); // '('
      // Parse arguments first so we can decide whether to build a Call or ObjectLiteral
      std::vector<std::unique_ptr<ast::Expr>> args;
      if (peek().kind != TK::RParen) {
        for (;;) {
          auto arg = parseExpr();
          args.emplace_back(std::move(arg));
          if (peek().kind == TK::Comma) { get(); continue; }
          break;
        }
      }
      expect(TK::RParen, "')'");

      // If callee is name 'object', desugar to ObjectLiteral
      if (base && base->kind == ast::NodeKind::Name) {
        const auto* nameExpr = dynamic_cast<const ast::Name*>(base.get());
        if (nameExpr != nullptr && nameExpr->id == "object") {
          auto obj = std::make_unique<ast::ObjectLiteral>();
          obj->fields = std::move(args);
          obj->line = base->line; obj->col = base->col; obj->file = base->file;
          base = std::move(obj);
          continue;
        }
      }

      auto call = std::make_unique<ast::Call>(std::move(base));
      call->args = std::move(args);
      base = std::move(call);
      continue;
    }
    break;
  }
  return base;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
ast::TypeKind Parser::toTypeKind(const std::string& typeName) {
  if (typeName == "int") { return ast::TypeKind::Int; }
  if (typeName == "bool") { return ast::TypeKind::Bool; }
  if (typeName == "float") { return ast::TypeKind::Float; }
  if (typeName == "str") { return ast::TypeKind::Str; }
  if (typeName == "None") { return ast::TypeKind::NoneType; }
  if (typeName == "tuple") { return ast::TypeKind::Tuple; }
  if (typeName == "list") { return ast::TypeKind::List; }
  if (typeName == "dict") { return ast::TypeKind::Dict; }
  return ast::TypeKind::NoneType;
}

} // namespace pycc::parse
