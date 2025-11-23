/***
 * Name: pycc::parse::Parser (impl)
 * Purpose: Minimal parser for Milestone 1.
 */
#include "parser/Parser.h"
#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/BinaryOperator.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/Expr.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ForStmt.h"
#include "ast/TryStmt.h"
#include "ast/ExceptHandler.h"
#include "ast/WithStmt.h"
#include "ast/WithItem.h"
#include "ast/Import.h"
#include "ast/ImportFrom.h"
#include "ast/Alias.h"
#include "ast/ClassDef.h"
#include "ast/DefStmt.h"
#include "ast/BreakStmt.h"
#include "ast/ContinueStmt.h"
#include "ast/PassStmt.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NodeKind.h"
#include "ast/NoneLiteral.h"
#include "ast/ObjectLiteral.h"
#include "ast/Param.h"
#include "ast/ReturnStmt.h"
#include "ast/Stmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/SetLiteral.h"
#include "ast/DictLiteral.h"
#include "ast/Comprehension.h"
#include "ast/AugAssignStmt.h"
#include "ast/RaiseStmt.h"
#include "ast/GlobalStmt.h"
#include "ast/NonlocalStmt.h"
#include "ast/AssertStmt.h"
#include "ast/YieldExpr.h"
#include "ast/AwaitExpr.h"
#include "ast/TypeKind.h"
#include "ast/Unary.h"
#include "ast/UnaryOperator.h"
#include "ast/MatchStmt.h"
#include "ast/Pattern.h"
#include "lexer/Lexer.h"
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pycc::parse {

using TK = lex::TokenKind;

void Parser::initBuffer() {
  if (initialized_) return;
  // Try to access the full token stream when backed by Lexer; otherwise, drain
  if (auto* lx = dynamic_cast<lex::Lexer*>(&ts_)) {
    tokens_ = lx->tokens();
  } else {
    // Fallback: consume from stream until End
    tokens_.clear();
    for (;;) {
      auto t = ts_.next();
      tokens_.push_back(t);
      if (t.kind == TK::End) break;
    }
  }
  pos_ = 0;
  initialized_ = true;
}

const lex::Token& Parser::peek() const {
  // Safe in presence of End sentry
  return tokens_[pos_ < tokens_.size() ? pos_ : (tokens_.size() - 1)];
}
const lex::Token& Parser::peekNext() const {
  const size_t idx = pos_ + 1;
  return tokens_[idx < tokens_.size() ? idx : (tokens_.size() - 1)];
}
lex::Token Parser::get() {
  if (pos_ < tokens_.size()) {
    return tokens_[pos_++];
  }
  return tokens_.empty() ? lex::Token{} : tokens_.back();
}

bool Parser::match(TK tokenKind) {
  if (peek().kind == tokenKind) { (void)get(); return true; }
  return false;
}

void Parser::recordExpectation(const char* msg) {
  if (pos_ >= farthestPos_) {
    farthestPos_ = pos_;
    farthestExpected_ = msg ? msg : "<token>";
  }
}

void Parser::addError(const std::string& msg) {
  hadErrors_ = true;
  // Build a context-rich message using current token
  const auto& got = peek();
  std::ostringstream head;
  head << "parse error: " << msg << " (got " << to_string(got.kind) << " '" << got.text << "')";
  errors_.push_back(formatContext(got, head.str()));
}

void Parser::synchronize() {
  // Delimiter-aware synchronization: balance (), [], {} while skipping ahead.
  int paren = 0;
  int bracket = 0;
  int brace = 0;
  for (;;) {
    const auto& t = peek();
    if (t.kind == TK::End) break;
    if (t.kind == TK::LParen) { ++paren; (void)get(); continue; }
    if (t.kind == TK::RParen) { if (paren > 0) --paren; (void)get(); continue; }
    if (t.kind == TK::LBracket) { ++bracket; (void)get(); continue; }
    if (t.kind == TK::RBracket) { if (bracket > 0) --bracket; (void)get(); continue; }
    if (t.kind == TK::LBrace) { ++brace; (void)get(); continue; }
    if (t.kind == TK::RBrace) { if (brace > 0) --brace; (void)get(); continue; }
    // When not nested inside delimiters, newline/dedent is a good boundary
    if (paren == 0 && bracket == 0 && brace == 0) {
      if (t.kind == TK::Newline || t.kind == TK::Dedent) { break; }
    }
    (void)get();
  }
  if (peek().kind == TK::Newline) { (void)get(); }
}

void Parser::synchronizeUntil(std::initializer_list<lex::TokenKind> terms) {
  int paren = 0;
  int bracket = 0;
  int brace = 0;
  auto isTerm = [&](lex::TokenKind k) {
    for (auto x : terms) if (x == k) return true; return false;
  };
  for (;;) {
    const auto& t = peek();
    if (t.kind == TK::End) break;
    if (t.kind == TK::LParen) { ++paren; (void)get(); continue; }
    if (t.kind == TK::RParen) {
      if (paren > 0) { --paren; (void)get(); continue; }
      if (paren == 0 && isTerm(TK::RParen)) break;
      (void)get(); continue;
    }
    if (t.kind == TK::LBracket) { ++bracket; (void)get(); continue; }
    if (t.kind == TK::RBracket) {
      if (bracket > 0) { --bracket; (void)get(); continue; }
      if (bracket == 0 && isTerm(TK::RBracket)) break;
      (void)get(); continue;
    }
    if (t.kind == TK::LBrace) { ++brace; (void)get(); continue; }
    if (t.kind == TK::RBrace) {
      if (brace > 0) { --brace; (void)get(); continue; }
      if (brace == 0 && isTerm(TK::RBrace)) break;
      (void)get(); continue;
    }
    if (paren == 0 && bracket == 0 && brace == 0 && isTerm(t.kind)) break;
    (void)get();
  }
}

void Parser::expect(TK tokenKind, const char* msg) {
  if (!match(tokenKind)) {
    recordExpectation(msg);
    const auto& got = peek();
    std::string m = "expected ";
    m += msg;
    m += ", got ";
    m += to_string(got.kind);
    m += " ('";
    m += got.text;
    m += "')";
    throw std::runtime_error(std::string("Parse error: ") + m);
  }
}

std::unique_ptr<ast::Module> Parser::parseModule() {
  initBuffer();
  auto mod = std::make_unique<ast::Module>();
  // Stamp module with source filename from the first token, if any
  if (!tokens_.empty()) {
    mod->file = tokens_[0].file;
    mod->line = 1; mod->col = 1;
  }
  while (peek().kind != TK::End) {
    if (peek().kind == TK::Newline) { get(); continue; }
    // Collect decorators if any, but recover if decorator expression parsing fails
    std::vector<std::unique_ptr<ast::Expr>> decorators;
    try {
      decorators = parseDecorators();
    } catch (const std::exception& ex) {
      addError(ex.what());
      synchronize();
      continue;
    }
    try {
      if (peek().kind == TK::Async && peekNext().kind == TK::Def) { get(); }
      if (peek().kind == TK::Def) {
        auto fn = parseFunction();
        fn->decorators = std::move(decorators);
        mod->functions.emplace_back(std::move(fn));
      } else if (peek().kind == TK::Class) {
        auto cls = parseClass();
        cls->decorators = std::move(decorators);
        mod->classes.emplace_back(std::move(cls));
      } else {
        // Fallback: attempt a statement at top-level; if it fails, recover
        (void)parseStatement(); // shape-only at top level; discard
      }
    } catch (const std::exception& ex) {
      addError(ex.what());
      synchronize();
    }
  }
  if (hadErrors_ || !errors_.empty()) {
    // Build a primary message at the farthest point, with context
    std::ostringstream oss;
    if (!farthestExpected_.empty() && !tokens_.empty()) {
      const size_t idx = (farthestPos_ < tokens_.size()) ? farthestPos_ : (tokens_.size() - 1);
      const auto& tok = tokens_[idx];
      std::ostringstream head;
      head << "expected " << farthestExpected_ << ", got " << to_string(tok.kind) << " '" << tok.text << "'";
      oss << formatContext(tok, head.str());
    } else {
      oss << errors_.front();
    }
    // Append notes for additional recovered errors (limit to 3)
    size_t notes = 0;
    for (const auto& e : errors_) {
      if (notes >= 3) break;
      // Avoid duplicating the primary if same text
      if (e == errors_.front()) continue;
      oss << "\nnote: " << e;
      ++notes;
    }
    throw std::runtime_error(oss.str());
  }
  return mod;
}

bool Parser::loadFileIfNeeded(const std::string& path) {
  if (path.empty()) return false;
  if (fileLines_.find(path) != fileLines_.end()) return true;
  std::ifstream in(path);
  if (!in) { return false; }
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) { lines.push_back(line); }
  fileLines_[path] = std::move(lines);
  return true;
}

std::string Parser::formatContext(const lex::Token& tok, const std::string& headMsg) const {
  std::ostringstream out;
  out << tok.file << ":" << tok.line << ":" << tok.col << ": " << headMsg;
  // Try to print the source line and a caret under the token
  auto it = fileLines_.find(tok.file);
  bool have = (it != fileLines_.end());
  if (!have) {
    // const_cast to call non-const helper via this; safe since we only populate cache
    auto* self = const_cast<Parser*>(this);
    have = self->loadFileIfNeeded(tok.file);
    it = self->fileLines_.find(tok.file);
  }
  if (have && tok.line > 0) {
    const auto& lines = it->second;
    if (static_cast<size_t>(tok.line) - 1 < lines.size()) {
      const std::string& srcLine = lines[static_cast<size_t>(tok.line) - 1];
      out << "\n" << srcLine;
      // Build caret line
      std::string caret;
      int col = tok.col;
      if (col < 1) col = 1;
      caret.assign(static_cast<size_t>(col - 1), ' ');
      caret.push_back('^');
      // underline token length if available
      const size_t len = tok.text.size();
      if (len > 1) caret.append(len - 1, '~');
      out << "\n" << caret;
    }
  }
  return out.str();
}

std::unique_ptr<ast::FunctionDef> Parser::parseFunction() {
  expect(TK::Def, "'def'");
  auto nameTok = get();
  if (nameTok.kind != TK::Ident) { throw std::runtime_error("Parse error: expected function name"); }
  expect(TK::LParen, "'('");
  std::vector<ast::Param> params;
  try {
    parseParamList(params);
  } catch (const std::exception& ex) {
    addError(ex.what());
    synchronizeUntil({TK::RParen, TK::Newline, TK::Dedent});
  }
  expect(TK::RParen, "')'");
  expect(TK::Arrow, "'->'");
  ast::TypeKind retKind = ast::TypeKind::NoneType;
  if (peek().kind == TK::LParen) {
    // Parenthesized type grouping: record first TypeIdent as return kind (shape-only)
    int depth = 1; get();
    bool set = false;
    while (depth > 0) {
      const auto& t = get();
      if (!set && t.kind == TK::TypeIdent) { retKind = toTypeKind(t.text); set = true; }
      if (t.kind == TK::LParen) ++depth;
      else if (t.kind == TK::RParen) --depth;
      else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) break;
    }
  } else {
    auto typeTok = get();
    if (typeTok.kind != TK::TypeIdent) { throw std::runtime_error("Parse error: expected return type ident"); }
    retKind = toTypeKind(typeTok.text);
    // Optional generic shape: list[int], dict[str, int], tuple[int, str]
    if (peek().kind == TK::LBracket) {
      int depth = 0;
      do {
        const auto& t = get();
        if (t.kind == TK::LBracket) { ++depth; }
        else if (t.kind == TK::RBracket) { --depth; }
        else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) { break; }
      } while (depth > 0);
    }
    // Accept union pipe syntax: T | U | ... (shape-only)
    while (peek().kind == TK::Pipe) {
      get(); // consume '|'
      const auto& typeTok2 = get();
      if (typeTok2.kind != TK::TypeIdent) { throw std::runtime_error("Parse error: expected type ident after '|'"); }
      if (peek().kind == TK::LBracket) {
        int d = 0; do { const auto& t = get(); if (t.kind == TK::LBracket) ++d; else if (t.kind == TK::RBracket) --d; else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) break; } while (d > 0);
      }
    }
  }
  expect(TK::Colon, "':'");
  if (peek().kind == TK::Newline) { get(); }

  auto func = std::make_unique<ast::FunctionDef>(nameTok.text, retKind);
  // Stamp function node with source location from the def name token
  func->file = nameTok.file;
  func->line = nameTok.line;
  func->col = nameTok.col;
  func->params = std::move(params);
  // Parse function suite via generic suite handler (tolerates leading comments/blank lines)
  parseSuiteInto(func->body);
  return func;
}

std::unique_ptr<ast::Stmt> Parser::parseStatement() {
  if (peek().kind == TK::Async && peekNext().kind == TK::Def) { get(); }
  if (peek().kind == TK::Def) {
    // Allow function defs as statements (e.g., in class bodies)
    auto fn = parseFunction();
    return std::make_unique<ast::DefStmt>(std::move(fn));
  }
  if (peek().kind == TK::Return) {
    const auto retTok = get();
    auto expr = parseExpr();
    // NEWLINE may follow (but parseFunction loop also handles)
    auto node = std::make_unique<ast::ReturnStmt>(std::move(expr));
    node->line = retTok.line; node->col = retTok.col; node->file = retTok.file;
    return node;
  }
  if (peek().kind == TK::Del) {
    get();
    auto del = std::make_unique<ast::DelStmt>();
    // parse one or more targets
    do {
      auto t = parsePostfix(parseUnary());
      // set delete ctx recursively on target shape (name/attr/subscript/tuple/list)
      setTargetContext(t.get(), ast::ExprContext::Del);
      del->targets.emplace_back(std::move(t));
      if (peek().kind != TK::Comma) break; get();
    } while (true);
    return del;
  }
  if (peek().kind == TK::Pass) { get(); return std::make_unique<ast::PassStmt>(); }
  if (peek().kind == TK::Break) { get(); return std::make_unique<ast::BreakStmt>(); }
  if (peek().kind == TK::Continue) { get(); return std::make_unique<ast::ContinueStmt>(); }
  if (peek().kind == TK::Assert) { return parseAssertStmt(); }
  if (peek().kind == TK::Raise) { return parseRaiseStmt(); }
  if (peek().kind == TK::Global) { return parseGlobalStmt(); }
  if (peek().kind == TK::Nonlocal) { return parseNonlocalStmt(); }
  if (peek().kind == TK::While) { return parseWhileStmt(); }
  if (peek().kind == TK::Async && peekNext().kind == TK::For) { get(); return parseForStmt(); }
  if (peek().kind == TK::For) { return parseForStmt(); }
  if (peek().kind == TK::Match) { return parseMatchStmt(); }
  if (peek().kind == TK::Try) { return parseTryStmt(); }
  if (peek().kind == TK::Async && peekNext().kind == TK::With) { get(); return parseWithStmt(); }
  if (peek().kind == TK::With) { return parseWithStmt(); }
  if (peek().kind == TK::Import || peek().kind == TK::From) { return parseImportStmt(); }
  if (peek().kind == TK::Class) { return parseClass(); }
  if (peek().kind == TK::If) { return parseIfStmt(); }
  // Augmented assignment (single target only)
  {
    lex::TokenKind which{};
    if (hasPendingAugAssignOnLine(which)) {
      auto target = parsePostfix(parseUnary());
      const auto opTok = get(); // the op-equal token
      (void)opTok;
      auto rhs = parseExpr();
      ast::BinaryOperator op = ast::BinaryOperator::Add;
      switch (which) {
        case TK::PlusEqual: op = ast::BinaryOperator::Add; break;
        case TK::MinusEqual: op = ast::BinaryOperator::Sub; break;
        case TK::StarEqual: op = ast::BinaryOperator::Mul; break;
        case TK::SlashEqual: op = ast::BinaryOperator::Div; break;
        case TK::SlashSlashEqual: op = ast::BinaryOperator::FloorDiv; break;
        case TK::PercentEqual: op = ast::BinaryOperator::Mod; break;
        case TK::StarStarEqual: op = ast::BinaryOperator::Pow; break;
        case TK::LShiftEqual: op = ast::BinaryOperator::LShift; break;
        case TK::RShiftEqual: op = ast::BinaryOperator::RShift; break;
        case TK::AmpEqual: op = ast::BinaryOperator::BitAnd; break;
        case TK::PipeEqual: op = ast::BinaryOperator::BitOr; break;
        case TK::CaretEqual: op = ast::BinaryOperator::BitXor; break;
        default: break;
      }
      if (!isValidAssignmentTarget(target.get())) { throw std::runtime_error("Parse error: invalid augmented assignment target"); }
      setTargetContext(target.get(), ast::ExprContext::Store);
      auto node = std::make_unique<ast::AugAssignStmt>(std::move(target), op, std::move(rhs));
      // Prefer the target's source location; fallback to operator token
      if (node->target) { node->line = node->target->line; node->col = node->target->col; node->file = node->target->file; }
      else { node->line = opTok.line; node->col = opTok.col; node->file = opTok.file; }
      return node;
    }
  }
  // Assignment (supports tuple/list/attr/subscript targets, possibly comma-separated)
  // Annotated assignment: NAME ':' type ['=' expr]
  if (peek().kind == TK::Ident && peekNext().kind == TK::Colon) {
    const auto& nameTok = get(); get(); // consume name and ':'
    // parse a type ident/name with optional generic shape
    ast::TypeKind tkind = ast::TypeKind::NoneType;
    const auto& tTok = get();
    if (tTok.kind == TK::TypeIdent) {
      tkind = toTypeKind(tTok.text);
      if (peek().kind == TK::LBracket) {
        int depth = 0;
        do {
          const auto& t = get();
          if (t.kind == TK::LBracket) { ++depth; }
          else if (t.kind == TK::RBracket) { --depth; }
          else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) { break; }
        } while (depth > 0);
      }
      // Accept union pipe chain
      while (peek().kind == TK::Pipe) {
        get(); const auto& tt = get(); if (tt.kind != TK::TypeIdent) throw std::runtime_error("Parse error: expected type ident after '|'");
        if (peek().kind == TK::LBracket) { int d=0; do { const auto& t = get(); if (t.kind==TK::LBracket) ++d; else if (t.kind==TK::RBracket) --d; else if (t.kind==TK::End||t.kind==TK::Newline||t.kind==TK::Dedent) break; } while (d>0); }
      }
    }
    auto nameExpr = std::make_unique<ast::Name>(nameTok.text);
    if (tkind != ast::TypeKind::NoneType) nameExpr->setType(tkind);
    if (peek().kind == TK::Equal) {
      get();
      auto rhs = parseExpr();
      auto asg = std::make_unique<ast::AssignStmt>(nameTok.text, std::move(rhs));
      asg->targets.emplace_back(std::move(nameExpr));
      asg->line = nameTok.line; asg->col = nameTok.col; asg->file = nameTok.file;
      return asg;
    }
    return std::make_unique<ast::ExprStmt>(std::move(nameExpr));
  }
  if (hasPendingEqualOnLine()) {
    std::vector<std::unique_ptr<ast::Expr>> targets;
    // Parse one or more targets separated by commas
    do {
      auto t = parsePostfix(parseUnary());
      targets.emplace_back(std::move(t));
      if (peek().kind != TK::Comma) break; get();
    } while (true);
    expect(TK::Equal, "'='");
    auto rhs = parseExpr();
    // Allow tuple without parens on RHS: a, b = 1, 2
    if (peek().kind == TK::Comma) {
      auto tup = std::make_unique<ast::TupleLiteral>();
      tup->elements.emplace_back(std::move(rhs));
      while (peek().kind == TK::Comma) { get(); tup->elements.emplace_back(parseExpr()); }
      rhs = std::move(tup);
    }
    // Validate and set ctx on all targets
    for (auto& t : targets) {
      if (!isValidAssignmentTarget(t.get())) { throw std::runtime_error("Parse error: invalid assignment target"); }
      setTargetContext(t.get(), ast::ExprContext::Store);
    }
    std::string legacy = "";
    if (targets.size() == 1 && targets[0]->kind == ast::NodeKind::Name) {
      legacy = static_cast<ast::Name*>(targets[0].get())->id;
    }
    auto asg = std::make_unique<ast::AssignStmt>(legacy, std::move(rhs));
    for (auto& t : targets) { asg->targets.emplace_back(std::move(t)); }
    if (!asg->targets.empty()) {
      const auto* t0 = asg->targets.front().get();
      asg->line = t0->line; asg->col = t0->col; asg->file = t0->file;
    }
    return asg;
  }
  // Fallback: expression statement
  auto expr = parseExpr();
  return std::make_unique<ast::ExprStmt>(std::move(expr));
}

void Parser::parseParamList(std::vector<ast::Param>& outParams) {
  if (peek().kind == TK::RParen) { return; }
  bool kwOnly = false;
  bool seenSlash = false;
  for (;;) {
    if (peek().kind == TK::StarStar) {
      get();
      const auto& nm = get(); if (nm.kind != TK::Ident) throw std::runtime_error("Parse error: expected name after '**'");
      ast::Param param; param.name = nm.text; param.isKwVarArg = true; parseOptionalParamType(param);
      outParams.push_back(std::move(param));
    } else if (peek().kind == TK::Star) {
      get();
      if (peek().kind == TK::Comma) { kwOnly = true; }
      else {
        const auto& nm = get(); if (nm.kind != TK::Ident) throw std::runtime_error("Parse error: expected name after '*'");
        ast::Param param; param.name = nm.text; param.isVarArg = true; parseOptionalParamType(param);
        outParams.push_back(std::move(param));
        kwOnly = true; // params after *args are kw-only
      }
    } else if (peek().kind == TK::Slash) {
      // Positional-only divider; mark all prior non-var params as positional-only
      get();
      if (seenSlash) { throw std::runtime_error("Parse error: duplicate '/' in parameter list"); }
      if (outParams.empty()) { throw std::runtime_error("Parse error: '/' requires at least one parameter before it"); }
      for (auto& p : outParams) {
        if (!p.isVarArg && !p.isKwVarArg) { p.isPosOnly = true; }
      }
      seenSlash = true;
      // Allow an optional trailing comma after '/'
      if (peek().kind == TK::Comma) { get(); if (peek().kind == TK::RParen) break; continue; }
      if (peek().kind == TK::RParen) { break; }
      // Else continue parsing next param normally
    } else {
      const auto& pNameTok = get();
      if (pNameTok.kind != TK::Ident) {
        addError("Parse error: expected parameter name");
        synchronizeUntil({TK::Comma, TK::RParen});
        if (peek().kind == TK::Comma) { get(); if (peek().kind == TK::RParen) break; continue; }
        break;
      }
      ast::Param param; param.name = pNameTok.text; param.type = ast::TypeKind::NoneType; param.isKwOnly = kwOnly;
      try { parseOptionalParamType(param); }
      catch (const std::exception& ex) { addError(ex.what()); synchronizeUntil({TK::Comma, TK::RParen}); }
      if (peek().kind == TK::Equal) {
        get();
        try { param.defaultValue = parseExpr(); }
        catch (const std::exception& ex) { addError(ex.what()); synchronizeUntil({TK::Comma, TK::RParen}); }
      }
      outParams.push_back(std::move(param));
    }
    if (peek().kind != TK::Comma) break; get(); if (peek().kind == TK::RParen) break;
  }
}

void Parser::parseOptionalParamType(ast::Param& param) {
  if (peek().kind != TK::Colon) { return; }
  get();
  if (peek().kind == TK::LParen) {
    // Parenthesized grouping: take first TypeIdent inside as shape-only
    int depth = 1; get();
    bool set = false;
    while (depth > 0) {
      const auto& t = get();
      if (!set && t.kind == TK::TypeIdent) { param.type = toTypeKind(t.text); set = true; }
      if (t.kind == TK::LParen) ++depth;
      else if (t.kind == TK::RParen) --depth;
      else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) break;
    }
    if (!set) { param.type = ast::TypeKind::NoneType; }
    param.unionTypes.clear();
    param.unionTypes.push_back(param.type);
    return;
  }
  const auto& pTypeTok = get();
  if (pTypeTok.kind != TK::TypeIdent) { throw std::runtime_error("Parse error: expected type ident after ':'"); }
  param.type = toTypeKind(pTypeTok.text);
  param.unionTypes.clear();
  param.unionTypes.push_back(param.type);
  // Accept generic shape like list[int], dict[str, int], tuple[int, str]
  if (peek().kind == TK::LBracket) {
    // capture a simple single-type generic parameter for list[T]
    get(); // '['
    if (peek().kind == TK::TypeIdent) {
      const auto& inner = get();
      auto innerKind = toTypeKind(inner.text);
      if (param.type == ast::TypeKind::List) { param.listElemType = innerKind; }
    }
    // consume until ']'
    int depth = 1;
    while (depth > 0) {
      const auto& t = get();
      if (t.kind == TK::LBracket) ++depth; else if (t.kind == TK::RBracket) --depth; else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) break;
    }
  }
  // Accept union pipe chain: T | U | V (shape-only)
  while (peek().kind == TK::Pipe) {
    get();
    const auto& nextTok = get();
    if (nextTok.kind != TK::TypeIdent) { throw std::runtime_error("Parse error: expected type ident after '|'"); }
    param.unionTypes.push_back(toTypeKind(nextTok.text));
    // Optional bracketed shape
    if (peek().kind == TK::LBracket) {
      int d = 1; get();
      while (d > 0) { const auto& t = get(); if (t.kind == TK::LBracket) ++d; else if (t.kind == TK::RBracket) --d; else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) break; }
    }
  }
  // Accept parenthesized grouping for type annotations: NAME: (T | U)
  if (peek().kind == TK::LParen) {
    int depth = 1; get();
    while (depth > 0) {
      const auto& t = get();
      if (t.kind == TK::LParen) ++depth;
      else if (t.kind == TK::RParen) --depth;
      else if (t.kind == TK::End || t.kind == TK::Newline || t.kind == TK::Dedent) break;
    }
  }
}

std::unique_ptr<ast::Stmt> Parser::parseIfStmt() {
  get();
  auto cond = parseExpr();
  // Tolerate a missing ':' before the function body in demos; record an error but continue.
  if (peek().kind == TK::Colon) { get(); }
  else { addError("expected ':'"); }
  expect(TK::Newline, "newline");
  if (peek().kind == TK::Indent) { get(); }
  auto ifs = std::make_unique<ast::IfStmt>(std::move(cond));
  parseSuiteInto(ifs->thenBody);
  // elif chain as nested IfStmt in else
  ast::IfStmt* cur = ifs.get();
  while (peek().kind == TK::Elif) {
    get();
    auto econd = parseExpr();
    expect(TK::Colon, ":'");
    expect(TK::Newline, "newline");
    if (peek().kind == TK::Indent) { get(); }
    auto elifNode = std::make_unique<ast::IfStmt>(std::move(econd));
    parseSuiteInto(elifNode->thenBody);
    cur->elseBody.emplace_back(std::move(elifNode));
    cur = static_cast<ast::IfStmt*>(cur->elseBody.back().get());
  }
  if (peek().kind == TK::Else) {
    get();
    expect(TK::Colon, ":'");
    expect(TK::Newline, "newline");
    if (peek().kind == TK::Indent) { get(); }
    parseSuiteInto(cur->elseBody);
  }
  return ifs;
}

void Parser::parseSuiteInto(std::vector<std::unique_ptr<ast::Stmt>>& out) {
  // Allow a stray Indent to appear after skipping blank/comment lines
  if (peek().kind == TK::Indent) { get(); }
  while (peek().kind != TK::Dedent && peek().kind != TK::End) {
    if (peek().kind == TK::Newline) { get(); continue; }
    // Tolerate an Indent token that may arrive after leading comments
    if (peek().kind == TK::Indent) { get(); continue; }
    if (peek().kind == TK::At) {
      auto decorators = parseDecorators();
      if (peek().kind == TK::Def) {
        auto fn = parseFunction();
        fn->decorators = std::move(decorators);
        out.emplace_back(std::make_unique<ast::DefStmt>(std::move(fn)));
        continue;
      } else if (peek().kind == TK::Class) {
        auto cls = parseClass();
        cls->decorators = std::move(decorators);
        out.emplace_back(std::move(cls));
        continue;
      }
      throw std::runtime_error("Parse error: decorators must precede 'def' or 'class'");
    }
    out.emplace_back(parseStatement());
  }
  expect(TK::Dedent, "dedent");
}

std::unique_ptr<ast::Expr> Parser::parseExpr() {
  // Named expression: NAME := expr (shape-only; NAME required)
  if (peek().kind == TK::Ident && peekNext().kind == TK::ColonEqual) {
    const auto nameTok = get(); // NAME
    (void)get(); // ':='
    auto rhs = parseExpr();
    auto node = std::make_unique<ast::NamedExpr>(nameTok.text, std::move(rhs));
    node->line = nameTok.line; node->col = nameTok.col; node->file = nameTok.file;
    return node;
  }
  // Conditional expression: <expr> if <expr> else <expr>
  auto condBase = parseLogicalOr();
  if (peek().kind == TK::If) {
    // Attempt PEG-style: commit only if we see a matching 'else'
    const auto m = mark();
    get(); // 'if'
    auto test = parseExpr();
    if (match(TK::Else)) {
      auto orelse = parseExpr();
      return std::make_unique<ast::IfExpr>(std::move(condBase), std::move(test), std::move(orelse));
    }
    // Backtrack: not a conditional expression
    rewind(m);
  }
  return condBase;
}

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
  auto left = parseBitwiseOr();
  using TK = lex::TokenKind;
  // collect chain
  std::vector<ast::BinaryOperator> ops;
  std::vector<std::unique_ptr<ast::Expr>> comps;
  for (;;) {
    const auto& k = peek().kind;
    if (!(k == TK::EqEq || k == TK::NotEq || k == TK::Lt || k == TK::Le || k == TK::Gt || k == TK::Ge || k == TK::Is || k == TK::In || (k == TK::Not && peekNext().kind == TK::In))) {
      break;
    }
    auto opTok = get();
    ast::BinaryOperator binOp = ast::BinaryOperator::Eq;
    if (opTok.kind == TK::Not && peek().kind == TK::In) { get(); binOp = ast::BinaryOperator::NotIn; }
    else {
      switch (opTok.kind) {
        case TK::EqEq: binOp = ast::BinaryOperator::Eq; break;
        case TK::NotEq: binOp = ast::BinaryOperator::Ne; break;
        case TK::Lt: binOp = ast::BinaryOperator::Lt; break;
        case TK::Le: binOp = ast::BinaryOperator::Le; break;
        case TK::Gt: binOp = ast::BinaryOperator::Gt; break;
        case TK::Ge: binOp = ast::BinaryOperator::Ge; break;
        case TK::Is: binOp = (peek().kind == TK::Not ? (get(), ast::BinaryOperator::IsNot) : ast::BinaryOperator::Is); break;
        case TK::In: binOp = ast::BinaryOperator::In; break;
        default: break;
      }
    }
    ops.push_back(binOp);
    comps.emplace_back(parseBitwiseOr());
  }
  if (ops.empty()) { return left; }
  if (ops.size() == 1) {
    // backward-compat: emit Binary when single comparator
    auto node = std::make_unique<ast::Binary>(ops[0], std::move(left), std::move(comps[0]));
    return node;
  }
  auto cmp = std::make_unique<ast::Compare>();
  cmp->left = std::move(left);
  cmp->ops = std::move(ops);
  cmp->comparators = std::move(comps);
  return cmp;
}

std::unique_ptr<ast::Expr> Parser::parseBitwiseOr() {
  auto lhs = parseBitwiseXor();
  while (peek().kind == TK::Pipe) {
    auto tok = get();
    auto rhs = parseBitwiseXor();
    auto node = std::make_unique<ast::Binary>(ast::BinaryOperator::BitOr, std::move(lhs), std::move(rhs));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    lhs = std::move(node);
  }
  return lhs;
}

std::unique_ptr<ast::Expr> Parser::parseBitwiseXor() {
  auto lhs = parseBitwiseAnd();
  while (peek().kind == TK::Caret) {
    auto tok = get();
    auto rhs = parseBitwiseAnd();
    auto node = std::make_unique<ast::Binary>(ast::BinaryOperator::BitXor, std::move(lhs), std::move(rhs));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    lhs = std::move(node);
  }
  return lhs;
}

std::unique_ptr<ast::Expr> Parser::parseBitwiseAnd() {
  auto lhs = parseShift();
  while (peek().kind == TK::Amp) {
    auto tok = get();
    auto rhs = parseShift();
    auto node = std::make_unique<ast::Binary>(ast::BinaryOperator::BitAnd, std::move(lhs), std::move(rhs));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    lhs = std::move(node);
  }
  return lhs;
}

std::unique_ptr<ast::Expr> Parser::parseShift() {
  auto lhs = parseAdditive();
  while (peek().kind == TK::LShift || peek().kind == TK::RShift) {
    auto tok = get();
    auto rhs = parseAdditive();
    auto op = (tok.kind == TK::LShift) ? ast::BinaryOperator::LShift : ast::BinaryOperator::RShift;
    auto node = std::make_unique<ast::Binary>(op, std::move(lhs), std::move(rhs));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    lhs = std::move(node);
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

std::unique_ptr<ast::Expr> Parser::parseMultiplicative() {
  // Allow postfix on unary (calls) before binding operators
  auto lhs = parsePostfix(parseUnary());
  for (;;) {
    const auto kind = peek().kind;
    if (!(kind == TK::Star || kind == TK::Slash || kind == TK::Percent || kind == TK::SlashSlash || kind == TK::StarStar)) { break; }
    auto opTok = get();
    auto rhs = parsePostfix(parseUnary());
    const ast::BinaryOperator binOp = mulOpFor(opTok.kind);
    auto node = std::make_unique<ast::Binary>(binOp, std::move(lhs), std::move(rhs));
    node->line = opTok.line; node->col = opTok.col; node->file = opTok.file;
    lhs = std::move(node);
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
  if (peek().kind == TK::Tilde) {
    auto tok = get();
    auto operand = parsePostfix(parsePrimary());
    auto node = std::make_unique<ast::Unary>(ast::UnaryOperator::BitNot, std::move(operand));
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  if (peek().kind == TK::Plus) {
    get(); // unary plus: no-op
    return parsePostfix(parsePrimary());
  }
  if (peek().kind == TK::Await) {
    auto tok = get();
    auto operand = parsePostfix(parsePrimary());
    auto node = std::make_unique<ast::AwaitExpr>();
    node->value = std::move(operand);
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  return parsePrimary();
}

std::unique_ptr<ast::Expr> Parser::parsePrimary() {
  const auto& tok = get();
  if (tok.kind == TK::Lambda) {
    // lambda [params] : expr
    std::vector<std::string> params;
    if (peek().kind != TK::Colon) {
      // parse parameter names separated by commas
      for (;;) {
        const auto& pn = get();
        if (pn.kind != TK::Ident) { throw std::runtime_error("Parse error: expected parameter name in lambda"); }
        params.push_back(pn.text);
        if (peek().kind != TK::Comma) break; get();
      }
    }
    expect(TK::Colon, ":'");
    auto body = parseExpr();
    auto lam = std::make_unique<ast::LambdaExpr>();
    lam->params = std::move(params);
    lam->body = std::move(body);
    lam->line = tok.line; lam->col = tok.col; lam->file = tok.file;
    return lam;
  }
  if (tok.kind == TK::Int) { auto node = std::make_unique<ast::IntLiteral>(std::stoll(tok.text)); node->line = tok.line; node->col = tok.col; node->file = tok.file; return node; }
  if (tok.kind == TK::Float) { auto node = std::make_unique<ast::FloatLiteral>(std::stod(tok.text)); node->line = tok.line; node->col = tok.col; node->file = tok.file; return node; }
  if (tok.kind == TK::Imag) { auto node = std::make_unique<ast::ImagLiteral>(std::stod(tok.text)); node->line = tok.line; node->col = tok.col; node->file = tok.file; return node; }
  if (tok.kind == TK::String) {
    auto raw = tok.text;
    auto hasF = [&](){ size_t i = 0; auto pref=[&](char c){return c=='f'||c=='F'||c=='r'||c=='R'||c=='b'||c=='B';}; if (i<raw.size() && pref(raw[i])) { bool f = (raw[i]=='f'||raw[i]=='F'); ++i; if (i<raw.size() && pref(raw[i])) { f = f || (raw[i]=='f'||raw[i]=='F'); ++i; } return f; } return false; }();
    if (hasF) {
      auto content = unquoteString(raw);
      auto fs = std::make_unique<ast::FStringLiteral>();
      std::string lit;
      size_t i = 0; auto n = content.size();
      auto flush_lit = [&](){ if (!lit.empty()) { ast::FStringSegment s; s.isExpr=false; s.text = lit; fs->parts.push_back(std::move(s)); lit.clear(); } };
      while (i < n) {
        if (content[i] == '{') {
          if (i+1 < n && content[i+1] == '{') { lit.push_back('{'); i += 2; continue; }
          // parse expression segment
          size_t j = i + 1; int depth = 1; size_t exprEnd = std::string::npos; const size_t pos = j; size_t topCut = std::string::npos;
          while (j < n && depth > 0) {
            if (content[j] == '{') { ++depth; }
            else if (content[j] == '}') { --depth; if (depth == 0) break; }
            else if ((content[j] == '!' || content[j] == ':') && depth == 1 && topCut == std::string::npos) { topCut = j; }
            ++j;
          }
          if (j >= n || depth != 0) { throw std::runtime_error("Parse error: unclosed f-string expression"); }
          exprEnd = (topCut != std::string::npos ? topCut : j);
          const std::string exprText = content.substr(pos, exprEnd - pos);
          flush_lit();
          ast::FStringSegment seg; seg.isExpr = true; seg.expr = parseExprFromString(exprText, "<fexpr>"); fs->parts.push_back(std::move(seg));
          i = j + 1; // skip closing '}'
          continue;
        } else if (content[i] == '}') {
          if (i+1 < n && content[i+1] == '}') { lit.push_back('}'); i += 2; continue; }
          throw std::runtime_error("Parse error: single '}' in f-string");
        } else {
          lit.push_back(content[i]); ++i;
        }
      }
      flush_lit();
      fs->line = tok.line; fs->col = tok.col; fs->file = tok.file; return fs;
    }
    auto node = std::make_unique<ast::StringLiteral>(unquoteString(raw)); node->line = tok.line; node->col = tok.col; node->file = tok.file; return node;
  }
  if (tok.kind == TK::Bytes) { auto node = std::make_unique<ast::BytesLiteral>(unquoteString(tok.text.substr(1))); node->line = tok.line; node->col = tok.col; node->file = tok.file; return node; }
  if (tok.kind == TK::Ellipsis) { auto node = std::make_unique<ast::EllipsisLiteral>(); node->line = tok.line; node->col = tok.col; node->file = tok.file; return node; }
  if (tok.kind == TK::Ident || tok.kind == TK::TypeIdent) { return parseNameOrNone(tok); }
  if (tok.kind == TK::BoolLit) { const bool isTrue = (tok.text == "True"); auto node = std::make_unique<ast::BoolLiteral>(isTrue); node->line = tok.line; node->col = tok.col; node->file = tok.file; return node; }
  if (tok.kind == TK::LBracket) { return parseListLiteral(tok); }
  if (tok.kind == TK::LBrace) { return parseDictOrSetLiteral(tok); }
  if (tok.kind == TK::Yield) {
    auto y = std::make_unique<ast::YieldExpr>();
    if (peek().kind == TK::From) { get(); y->isFrom = true; y->value = parseExpr(); }
    else if (peek().kind != TK::Newline && peek().kind != TK::Comma && peek().kind != TK::RParen) { y->value = parseExpr(); }
    y->line = tok.line; y->col = tok.col; y->file = tok.file; return y;
  }
  if (tok.kind == TK::LParen) { return parseTupleOrParen(tok); }
  throw std::runtime_error("Parse error: expected primary expression");
}

std::unique_ptr<ast::Expr> Parser::parsePostfix(std::unique_ptr<ast::Expr> base) {
  for (;;) {
    if (peek().kind == TK::LParen) {
      get();
      auto argres = parseArgList();
      if (argres.keywords.empty() && argres.starArgs.empty() && argres.kwStarArgs.empty()) {
        if (desugarObjectCall(base, argres.positional)) { continue; }
      }
      auto call = std::make_unique<ast::Call>(std::move(base));
      call->args = std::move(argres.positional);
      call->keywords = std::move(argres.keywords);
      call->starArgs = std::move(argres.starArgs);
      call->kwStarArgs = std::move(argres.kwStarArgs);
      base = std::move(call);
      continue;
    }
    if (peek().kind == TK::Dot) {
      get(); const auto& nm = get(); if (nm.kind != TK::Ident) throw std::runtime_error("Parse error: expected attribute name");
      base = std::make_unique<ast::Attribute>(std::move(base), nm.text);
      continue;
    }
    if (peek().kind == TK::LBracket) {
      get();
      // parse subscript or slice: a[b] or a[b:c[:d]] or a[b, c] or a[:]
      std::unique_ptr<ast::Expr> first;
      bool isSlice = false;
      if (peek().kind == TK::Colon) {
        isSlice = true; // a[:...]
      } else if (peek().kind != TK::RBracket) {
        first = parseExpr();
        if (peek().kind == TK::Colon) { isSlice = true; }
      }
      if (isSlice) {
        auto tup = std::make_unique<ast::TupleLiteral>();
        // lower
        tup->elements.emplace_back(first ? std::move(first) : std::make_unique<ast::NoneLiteral>());
        if (peek().kind == TK::Colon) get(); // ':'
        // upper
        if (peek().kind != TK::Colon && peek().kind != TK::RBracket) { tup->elements.emplace_back(parseExpr()); }
        else { tup->elements.emplace_back(std::make_unique<ast::NoneLiteral>()); }
        // step
        if (peek().kind == TK::Colon) { get(); if (peek().kind != TK::RBracket) tup->elements.emplace_back(parseExpr()); else tup->elements.emplace_back(std::make_unique<ast::NoneLiteral>()); }
        expect(TK::RBracket, "]'");
        base = std::make_unique<ast::Subscript>(std::move(base), std::move(tup));
      } else {
        // multi-index a[b, c] -> TupleLiteral slice
        if (peek().kind == TK::Comma) {
          auto tup = std::make_unique<ast::TupleLiteral>();
          if (first) tup->elements.emplace_back(std::move(first));
          while (peek().kind == TK::Comma) {
            get();
            if (peek().kind == TK::RBracket) break;
            tup->elements.emplace_back(parseExpr());
          }
          expect(TK::RBracket, "]'");
          base = std::make_unique<ast::Subscript>(std::move(base), std::move(tup));
        } else {
          expect(TK::RBracket, "]'");
          base = std::make_unique<ast::Subscript>(std::move(base), std::move(first));
        }
      }
      continue;
    }
    break;
  }
  return base;
}

// New statement helpers (shape-only) defined below in this file

// Helpers
std::string Parser::unquoteString(std::string text) {
  // Handle optional prefixes: [fFrRbB]{1,2}
  size_t i = 0; auto isPref=[&](char c){return c=='f'||c=='F'||c=='r'||c=='R'||c=='b'||c=='B';};
  if (text.size()>=1 && isPref(text[i])) { ++i; if (text.size()>i && isPref(text[i])) ++i; }
  if (i >= text.size()) return {};
  // Determine quote style
  if (i+2 < text.size() && (text[i]=='\'' || text[i]=='"') && text[i]==text[i+1] && text[i]==text[i+2]) {
    // triple quoted
    const char q = text[i];
    const size_t start = i+3; size_t end = text.size();
    if (end >= 3 && text[end-1]==q && text[end-2]==q && text[end-3]==q) end -= 3; // trim
    if (end >= start) return text.substr(start, end-start);
    return {};
  }
  if (text[i]=='\'' || text[i]=='"') {
    const char q=text[i]; const size_t start=i+1; size_t end=text.size(); if (end>i+1 && text[end-1]==q) --end; if (end>=start) return text.substr(start, end-start);
  }
  return text;
}

std::unique_ptr<ast::Expr> Parser::parseExprFromString(const std::string& text,
                                                      const std::string& name) {
  lex::Lexer L; L.pushString(text, name);
  Parser P(L);
  P.initBuffer();
  return P.parseExpr();
}

// Static public entrypoint used by compile-time evaluation paths
std::unique_ptr<ast::Expr> Parser::parseSmallExprFromString(const std::string& text,
                                                            const std::string& name) {
  lex::Lexer L; L.pushString(text, name);
  Parser P(L);
  P.initBuffer();
  return P.parseExpr();
}

std::unique_ptr<ast::Expr> Parser::parseListLiteral(const lex::Token& openTok) {
  if (peek().kind == TK::RBracket) {
    auto list = std::make_unique<ast::ListLiteral>();
    expect(TK::RBracket, "]'");
    list->line = openTok.line; list->col = openTok.col; list->file = openTok.file;
    return list;
  }
  // First element/expression
  auto first = parseExpr();
  // List comprehension if 'for' or 'async for' follows
  if (peek().kind == TK::For || (peek().kind == TK::Async && peekNext().kind == TK::For)) {
    // std::cerr << "parseListLiteral: entering list comp\n";
    auto lc = std::make_unique<ast::ListComp>();
    lc->elt = std::move(first);
    lc->fors = parseComprehensionFors();
    expect(TK::RBracket, "]'");
    lc->line = openTok.line; lc->col = openTok.col; lc->file = openTok.file;
    return lc;
  }
  auto list = std::make_unique<ast::ListLiteral>();
  list->elements.emplace_back(std::move(first));
  while (peek().kind == TK::Comma) {
    get();
    if (peek().kind == TK::RBracket) { throw std::runtime_error("Parse error: trailing comma in list literal not allowed"); }
    list->elements.emplace_back(parseExpr());
  }
  expect(TK::RBracket, "]'");
  list->line = openTok.line; list->col = openTok.col; list->file = openTok.file;
  return list;
}

// Collect decorators: a sequence of '@' expr [NEWLINE]
std::vector<std::unique_ptr<ast::Expr>> Parser::parseDecorators() {
  std::vector<std::unique_ptr<ast::Expr>> decs;
  while (peek().kind == TK::At) {
    get();
    // Defensive: decorator expressions should be a single logical line; try/catch to recover
    try {
      decs.emplace_back(parseExpr());
    } catch (const std::exception& ex) {
      addError(ex.what());
      synchronize();
      continue;
    }
    if (peek().kind == TK::Newline) { get(); }
  }
  return decs;
}

// Parse a brace literal: dict, set, or their comprehensions
std::unique_ptr<ast::Expr> Parser::parseDictOrSetLiteral(const lex::Token& openTok) {
  // '{}' -> empty dict
  if (peek().kind == TK::RBrace) {
    get(); auto d = std::make_unique<ast::DictLiteral>(); d->line = openTok.line; d->col = openTok.col; d->file = openTok.file; return d;
  }
  // If starts with '**' -> dict with unpack(s)
  if (peek().kind == TK::StarStar) {
    auto dict = std::make_unique<ast::DictLiteral>();
    bool first = true;
    for (;;) {
      if (!first) {
        if (peek().kind == TK::RBrace) break;
        expect(TK::Comma, "','");
        if (peek().kind == TK::RBrace) break;
      }
      if (peek().kind == TK::StarStar) { get(); dict->unpacks.emplace_back(parseExpr()); }
      else { auto k = parseExpr(); expect(TK::Colon, ":'"); auto v = parseExpr(); dict->items.emplace_back(std::move(k), std::move(v)); }
      first = false;
    }
    expect(TK::RBrace, "'}'"); dict->line = openTok.line; dict->col = openTok.col; dict->file = openTok.file; return dict;
  }
  // Parse first element
  auto first = parseExpr();
  // Dict: first ':' value
  if (peek().kind == TK::Colon) {
    get(); auto firstVal = parseExpr();
  // Dict comprehension? (allow 'async for')
  if (peek().kind == TK::For || (peek().kind == TK::Async && peekNext().kind == TK::For)) {
      auto dc = std::make_unique<ast::DictComp>();
      dc->key = std::move(first);
      dc->value = std::move(firstVal);
      dc->fors = parseComprehensionFors();
      expect(TK::RBrace, "'}'");
      dc->line = openTok.line; dc->col = openTok.col; dc->file = openTok.file;
      return dc;
    }
    auto dict = std::make_unique<ast::DictLiteral>();
    dict->items.emplace_back(std::move(first), std::move(firstVal));
    while (peek().kind == TK::Comma) {
      get();
      if (peek().kind == TK::RBrace) break;
      if (peek().kind == TK::StarStar) { get(); dict->unpacks.emplace_back(parseExpr()); continue; }
      auto k = parseExpr(); expect(TK::Colon, ":'"); auto v = parseExpr(); dict->items.emplace_back(std::move(k), std::move(v));
    }
    expect(TK::RBrace, "'}'");
    dict->line = openTok.line; dict->col = openTok.col; dict->file = openTok.file;
    return dict;
  }
  // Set comprehension? (allow 'async for')
  if (peek().kind == TK::For || (peek().kind == TK::Async && peekNext().kind == TK::For)) {
    auto sc = std::make_unique<ast::SetComp>();
    sc->elt = std::move(first);
    sc->fors = parseComprehensionFors();
    expect(TK::RBrace, "'}'");
    sc->line = openTok.line; sc->col = openTok.col; sc->file = openTok.file;
    return sc;
  }
  // Otherwise: set literal
  auto set = std::make_unique<ast::SetLiteral>();
  set->elements.emplace_back(std::move(first));
  while (peek().kind == TK::Comma) { get(); if (peek().kind == TK::RBrace) break; set->elements.emplace_back(parseExpr()); }
  expect(TK::RBrace, "'}'");
  set->line = openTok.line; set->col = openTok.col; set->file = openTok.file;
  return set;
}

// Parse comprehension tails: one or more 'for <target> in <iter> [if <expr>]*'
std::vector<ast::ComprehensionFor> Parser::parseComprehensionFors() {
  std::vector<ast::ComprehensionFor> out;
  while (peek().kind == TK::For || (peek().kind == TK::Async && peekNext().kind == TK::For)) {
    bool isAsync = false;
    if (peek().kind == TK::Async) { get(); isAsync = true; }
    expect(TK::For, "'for'");
    ast::ComprehensionFor cf;
    cf.isAsync = isAsync;
    // Target (allow destructuring target list shape)
    cf.target = parsePostfix(parseUnary());
    expect(TK::In, "'in'");
    cf.iter = parseExpr();
    while (peek().kind == TK::If) {
      get();
      // std::cerr << "comp for: guard begins\n";
      cf.ifs.emplace_back(parseExpr());
      // std::cerr << "comp for: guard parsed\n";
    }
    out.emplace_back(std::move(cf));
  }
  return out;
}

std::unique_ptr<ast::Stmt> Parser::parseRaiseStmt() {
  const auto tok = get(); // 'raise'
  auto rs = std::make_unique<ast::RaiseStmt>();
  if (peek().kind != TK::Newline && peek().kind != TK::Dedent) {
    rs->exc = parseExpr();
    if (peek().kind == TK::From) { get(); rs->cause = parseExpr(); }
  }
  rs->line = tok.line; rs->col = tok.col; rs->file = tok.file;
  return rs;
}

std::unique_ptr<ast::Stmt> Parser::parseGlobalStmt() {
  get(); auto gs = std::make_unique<ast::GlobalStmt>();
  for (;;) { const auto& nm = get(); if (nm.kind != TK::Ident) break; gs->names.emplace_back(nm.text); if (peek().kind != TK::Comma) break; get(); }
  return gs;
}

std::unique_ptr<ast::Stmt> Parser::parseNonlocalStmt() {
  get(); auto ns = std::make_unique<ast::NonlocalStmt>();
  for (;;) { const auto& nm = get(); if (nm.kind != TK::Ident) break; ns->names.emplace_back(nm.text); if (peek().kind != TK::Comma) break; get(); }
  return ns;
}

std::unique_ptr<ast::Stmt> Parser::parseAssertStmt() {
  get(); auto as = std::make_unique<ast::AssertStmt>();
  as->test = parseExpr();
  if (peek().kind == TK::Comma) { get(); as->msg = parseExpr(); }
  return as;
}

// Recursively set ExprContext on valid assignment/delete targets
void Parser::setTargetContext(ast::Expr* e, ast::ExprContext ctx) {
  if (e == nullptr) return;
  using NK = ast::NodeKind;
  switch (e->kind) {
    case NK::Name:
      static_cast<ast::Name*>(e)->ctx = ctx;
      break;
    case NK::Attribute:
      static_cast<ast::Attribute*>(e)->ctx = ctx;
      break;
    case NK::Subscript:
      static_cast<ast::Subscript*>(e)->ctx = ctx;
      break;
    case NK::TupleLiteral: {
      auto* tup = static_cast<ast::TupleLiteral*>(e);
      for (auto& el : tup->elements) { setTargetContext(el.get(), ctx); }
      break;
    }
    case NK::ListLiteral: {
      auto* lst = static_cast<ast::ListLiteral*>(e);
      for (auto& el : lst->elements) { setTargetContext(el.get(), ctx); }
      break;
    }
    default:
      break;
  }
}

// Validate assignment targets recursively (name, attr, subscript, tuple, list)
bool Parser::isValidAssignmentTarget(const ast::Expr* e) {
  if (!e) return false;
  using NK = ast::NodeKind;
  switch (e->kind) {
    case NK::Name:
    case NK::Attribute:
    case NK::Subscript:
      return true;
    case NK::TupleLiteral: {
      const auto* tup = static_cast<const ast::TupleLiteral*>(e);
      for (const auto& el : tup->elements) {
        if (!isValidAssignmentTarget(el.get())) return false;
      }
      return true;
    }
    case NK::ListLiteral: {
      const auto* lst = static_cast<const ast::ListLiteral*>(e);
      for (const auto& el : lst->elements) {
        if (!isValidAssignmentTarget(el.get())) return false;
      }
      return true;
    }
    default:
      return false;
  }
}

// Look ahead for a top-level '=' on the current logical line
bool Parser::hasPendingEqualOnLine() {
  int depth = 0;
  for (size_t i = pos_; i < tokens_.size() && i < pos_ + 256; ++i) {
    const auto& t = tokens_[i];
    if (t.kind == TK::End) return false;
    if (t.kind == TK::Newline || t.kind == TK::Dedent) return false;
    if (t.kind == TK::Colon && depth == 0) return false;
    if (t.kind == TK::LParen || t.kind == TK::LBracket) { ++depth; continue; }
    if (t.kind == TK::RParen || t.kind == TK::RBracket) { if (depth > 0) --depth; continue; }
    if (t.kind == TK::Equal && depth == 0) return true;
  }
  return false;
}

bool Parser::hasPendingAugAssignOnLine(lex::TokenKind& which) {
  int depth = 0;
  for (size_t i = pos_; i < tokens_.size() && i < pos_ + 256; ++i) {
    const auto& t = tokens_[i];
    if (t.kind == TK::End) return false;
    if (t.kind == TK::Newline || t.kind == TK::Dedent) return false;
    if (t.kind == TK::Colon && depth == 0) return false;
    if (t.kind == TK::LParen || t.kind == TK::LBracket || t.kind == TK::LBrace) { ++depth; continue; }
    if (t.kind == TK::RParen || t.kind == TK::RBracket || t.kind == TK::RBrace) { if (depth > 0) --depth; continue; }
    switch (t.kind) {
      case TK::PlusEqual: case TK::MinusEqual: case TK::StarEqual: case TK::SlashEqual:
      case TK::SlashSlashEqual: case TK::PercentEqual: case TK::StarStarEqual:
      case TK::LShiftEqual: case TK::RShiftEqual: case TK::AmpEqual: case TK::PipeEqual: case TK::CaretEqual:
        if (depth == 0) { which = t.kind; return true; }
        break;
      default: break;
    }
  }
  return false;
}

std::unique_ptr<ast::Expr> Parser::parseTupleOrParen(const lex::Token& openTok) {
  auto first = parseExpr();
  // Generator expression: (expr for ...), also accepts 'async for'
  if (peek().kind == TK::For || (peek().kind == TK::Async && peekNext().kind == TK::For)) {
    auto gen = std::make_unique<ast::GeneratorExpr>();
    gen->elt = std::move(first);
    gen->fors = parseComprehensionFors();
    expect(TK::RParen, "')'");
    gen->line = openTok.line; gen->col = openTok.col; gen->file = openTok.file;
    return gen;
  }
  if (peek().kind != TK::Comma) { expect(TK::RParen, "')'"); return first; }
  auto tup = std::make_unique<ast::TupleLiteral>();
  tup->elements.emplace_back(std::move(first));
  while (peek().kind == TK::Comma) {
    get();
    if (peek().kind == TK::RParen) { break; }
    tup->elements.emplace_back(parseExpr());
  }
  expect(TK::RParen, "')'");
  tup->line = openTok.line; tup->col = openTok.col; tup->file = openTok.file;
  return tup;
}

std::unique_ptr<ast::Expr> Parser::parseNameOrNone(const lex::Token& tok) {
  if (tok.kind == TK::TypeIdent && tok.text == "None") {
    auto node = std::make_unique<ast::NoneLiteral>();
    node->line = tok.line; node->col = tok.col; node->file = tok.file;
    return node;
  }
  auto node = std::make_unique<ast::Name>(tok.text);
  node->line = tok.line; node->col = tok.col; node->file = tok.file;
  return node;
}

Parser::ArgList Parser::parseArgList() {
  ArgList out;
  bool seenKeyword = false;
  if (peek().kind != TK::RParen) {
    for (;;) {
      try {
        if (peek().kind == TK::StarStar) {
          get(); out.kwStarArgs.emplace_back(parseExpr());
        } else if (peek().kind == TK::Star) {
          get(); out.starArgs.emplace_back(parseExpr());
        } else if (peek().kind == TK::Ident && peekNext().kind == TK::Equal) {
          // keyword argument
          const auto nameTok = get(); get(); // consume '='
          ast::KeywordArg kw{nameTok.text, parseExpr()};
          out.keywords.emplace_back(std::move(kw));
          seenKeyword = true;
        } else {
          // positional argument
          if (seenKeyword) { throw std::runtime_error("Parse error: positional argument follows keyword argument"); }
          out.positional.emplace_back(parseExpr());
        }
      } catch (const std::exception& ex) {
        addError(ex.what());
        synchronizeUntil({TK::Comma, TK::RParen});
      }
      if (peek().kind != TK::Comma) break;
      get();
      if (peek().kind == TK::RParen) break; // allow trailing comma
    }
  }
  expect(TK::RParen, "')'");
  return out;
}

bool Parser::desugarObjectCall(std::unique_ptr<ast::Expr>& base,
                               std::vector<std::unique_ptr<ast::Expr>>& args) {
  if (base && base->kind == ast::NodeKind::Name) {
    const auto* nameExpr = dynamic_cast<const ast::Name*>(base.get());
    if (nameExpr != nullptr && nameExpr->id == "object") {
      auto obj = std::make_unique<ast::ObjectLiteral>();
      obj->fields = std::move(args);
      obj->line = base->line; obj->col = base->col; obj->file = base->file;
      base = std::move(obj);
      return true;
    }
  }
  return false;
}

ast::BinaryOperator Parser::mulOpFor(lex::TokenKind kind) {
  switch (kind) {
    case TK::Star: return ast::BinaryOperator::Mul;
    case TK::Slash: return ast::BinaryOperator::Div;
    case TK::SlashSlash: return ast::BinaryOperator::FloorDiv;
    case TK::StarStar: return ast::BinaryOperator::Pow;
    case TK::Percent: return ast::BinaryOperator::Mod;
    default: return ast::BinaryOperator::Mul;
  }
}


// New statement helpers (shape-only)
std::unique_ptr<ast::Stmt> Parser::parseWhileStmt() {
  get();
  auto cond = parseExpr();
  expect(TK::Colon, ":'");
  expect(TK::Newline, "newline");
  expect(TK::Indent, "indent");
  auto ws = std::make_unique<ast::WhileStmt>(std::move(cond));
  parseSuiteInto(ws->thenBody);
  if (peek().kind == TK::Else) { get(); expect(TK::Colon, ":'"); expect(TK::Newline, "newline"); expect(TK::Indent, "indent"); parseSuiteInto(ws->elseBody); }
  return ws;
}

std::unique_ptr<ast::Stmt> Parser::parseForStmt() {
  get();
  // Parse a general target list (supports comma-separated destructuring)
  std::vector<std::unique_ptr<ast::Expr>> lhsTargets;
  lhsTargets.emplace_back(parsePostfix(parseUnary()));
  while (peek().kind == TK::Comma) {
    get();
    if (peek().kind == TK::In) break; // tolerate trailing comma before 'in' (shape-only)
    lhsTargets.emplace_back(parsePostfix(parseUnary()));
  }
  std::unique_ptr<ast::Expr> target;
  if (lhsTargets.size() == 1) {
    target = std::move(lhsTargets[0]);
  } else {
    auto tup = std::make_unique<ast::TupleLiteral>();
    for (auto& e : lhsTargets) tup->elements.emplace_back(std::move(e));
    target = std::move(tup);
  }
  expect(TK::In, "'in'"); auto iter = parseExpr();
  expect(TK::Colon, ":'"); expect(TK::Newline, "newline"); expect(TK::Indent, "indent");
  if (!isValidAssignmentTarget(target.get())) { throw std::runtime_error("Parse error: invalid for-target"); }
  auto fs = std::make_unique<ast::ForStmt>(std::move(target), std::move(iter));
  // Validate and mark target context as Store (recursively for tuples/lists)
  if (!isValidAssignmentTarget(fs->target.get())) { throw std::runtime_error("Parse error: invalid for-target"); }
  setTargetContext(fs->target.get(), ast::ExprContext::Store);
  parseSuiteInto(fs->thenBody);
  if (peek().kind == TK::Else) { get(); expect(TK::Colon, ":'"); expect(TK::Newline, "newline"); expect(TK::Indent, "indent"); parseSuiteInto(fs->elseBody); }
  return fs;
}

std::unique_ptr<ast::Stmt> Parser::parseTryStmt() {
  get(); expect(TK::Colon, ":'"); if (peek().kind == TK::Newline) get(); expect(TK::Indent, "indent");
  auto ts = std::make_unique<ast::TryStmt>(); parseSuiteInto(ts->body);
  bool sawHandler = false;
  while (peek().kind == TK::Except) {
    get(); auto eh = std::make_unique<ast::ExceptHandler>();
    if (peek().kind != TK::Colon) { eh->type = parseExpr(); if (peek().kind == TK::As) { get(); const auto& nm = get(); if (nm.kind == TK::Ident) eh->name = nm.text; } }
    expect(TK::Colon, ":'"); if (peek().kind == TK::Newline) get(); expect(TK::Indent, "indent"); parseSuiteInto(eh->body);
    ts->handlers.emplace_back(std::move(eh)); sawHandler = true;
  }
  if (peek().kind == TK::Else) { get(); expect(TK::Colon, ":'"); if (peek().kind == TK::Newline) get(); expect(TK::Indent, "indent"); parseSuiteInto(ts->orelse); }
  if (peek().kind == TK::Finally) { get(); expect(TK::Colon, ":'"); if (peek().kind == TK::Newline) get(); expect(TK::Indent, "indent"); parseSuiteInto(ts->finalbody); }
  else if (!sawHandler) { throw std::runtime_error("Parse error: try requires except or finally"); }
  return ts;
}

std::unique_ptr<ast::Stmt> Parser::parseWithStmt() {
  get(); auto ws = std::make_unique<ast::WithStmt>();
  // parse at least one with-item
  while (true) {
    auto item = std::make_unique<ast::WithItem>(); item->context = parseExpr();
    if (peek().kind == TK::As) { get(); const auto& nm = get(); if (nm.kind == TK::Ident) item->asName = nm.text; }
    ws->items.emplace_back(std::move(item));
    if (peek().kind != TK::Comma) break; get();
  }
  expect(TK::Colon, ":'"); if (peek().kind == TK::Newline) get(); expect(TK::Indent, "indent"); parseSuiteInto(ws->body); return ws;
}

std::unique_ptr<ast::Stmt> Parser::parseMatchStmt() {
  expect(TK::Match, "'match'");
  auto subject = parseExpr();
  expect(TK::Colon, ":'");
  if (peek().kind == TK::Newline) { get(); }
  expect(TK::Indent, "indent");
  auto ms = std::make_unique<ast::MatchStmt>();
  ms->subject = std::move(subject);
  while (peek().kind == TK::Case) {
    ms->cases.emplace_back(parseMatchCase());
  }
  expect(TK::Dedent, "dedent");
  return ms;
}

std::unique_ptr<ast::MatchCase> Parser::parseMatchCase() {
  expect(TK::Case, "'case'");
  auto pat = parsePattern();
  std::unique_ptr<ast::Expr> guard;
  if (peek().kind == TK::If) { get(); guard = parseExpr(); }
  expect(TK::Colon, ":'");
  if (peek().kind == TK::Newline) { get(); }
  expect(TK::Indent, "indent");
  auto cs = std::make_unique<ast::MatchCase>();
  cs->pattern = std::move(pat);
  cs->guard = std::move(guard);
  parseSuiteInto(cs->body);
  return cs;
}

std::unique_ptr<ast::Pattern> Parser::parsePattern() {
  // or-pattern with optional trailing 'as name'
  auto base = parsePatternOr();
  if (peek().kind == TK::As) {
    get();
    const auto& nm = get();
    if (nm.kind != TK::Ident) { throw std::runtime_error("Parse error: expected name after 'as'"); }
    return std::make_unique<ast::PatternAs>(std::move(base), nm.text);
  }
  return base;
}

std::unique_ptr<ast::Pattern> Parser::parsePatternOr() {
  auto first = parseSimplePattern();
  if (peek().kind != TK::Pipe) { return first; }
  auto por = std::make_unique<ast::PatternOr>();
  por->patterns.emplace_back(std::move(first));
  while (peek().kind == TK::Pipe) {
    get();
    por->patterns.emplace_back(parseSimplePattern());
  }
  return por;
}

std::unique_ptr<ast::Pattern> Parser::parseSimplePattern() {
  const auto& tok = get();
  using TK = lex::TokenKind;
  // Wildcard
  if (tok.kind == TK::Ident && tok.text == "_") {
    return std::make_unique<ast::PatternWildcard>();
  }
  // Name capture (not followed by '(')
  if (tok.kind == TK::Ident && peek().kind != TK::LParen) {
    return std::make_unique<ast::PatternName>(tok.text);
  }
  // Sequence patterns: [pats] or (pats)
  if (tok.kind == TK::LBracket) {
    auto ps = std::make_unique<ast::PatternSequence>();
    ps->isList = true;
    bool first = true;
    while (peek().kind != TK::RBracket) {
      if (!first) { if (peek().kind != TK::Comma) break; get(); if (peek().kind == TK::RBracket) break; }
      if (peek().kind == TK::Star) {
        get(); const auto& nm = get(); if (!(nm.kind == TK::Ident || (nm.kind == TK::Ident && nm.text == "_"))) throw std::runtime_error("Parse error: expected name after '*'");
        ps->elements.emplace_back(std::make_unique<ast::PatternStar>(nm.text));
      } else {
        ps->elements.emplace_back(parsePattern());
      }
      first = false;
    }
    expect(TK::RBracket, "]'");
    return ps;
  }
  if (tok.kind == TK::LParen) {
    // grouping or tuple pattern
    auto ps = std::make_unique<ast::PatternSequence>(); ps->isList = false;
    bool first = true;
    while (peek().kind != TK::RParen) {
      if (!first) { if (peek().kind != TK::Comma) break; get(); if (peek().kind == TK::RParen) break; }
      if (peek().kind == TK::Star) {
        get(); const auto& nm = get(); if (nm.kind != TK::Ident) throw std::runtime_error("Parse error: expected name after '*'");
        ps->elements.emplace_back(std::make_unique<ast::PatternStar>(nm.text));
      } else {
        ps->elements.emplace_back(parsePattern());
      }
      first = false;
    }
    expect(TK::RParen, ")'");
    if (ps->elements.size() == 1 && ps->elements[0]->kind != ast::NodeKind::PatternStar) {
      // grouping only
      return std::move(ps->elements[0]);
    }
    return ps;
  }
  // Mapping pattern: {key: pattern, ...}
  if (tok.kind == TK::LBrace) {
    auto pm = std::make_unique<ast::PatternMapping>();
    if (peek().kind != TK::RBrace) {
      bool first = true;
      for (;;) {
        if (!first) { if (peek().kind != TK::Comma) break; get(); if (peek().kind == TK::RBrace) break; }
        if (peek().kind == TK::StarStar) {
          get(); const auto& nm = get(); if (nm.kind != TK::Ident) throw std::runtime_error("Parse error: expected name after '**'");
          pm->hasRest = true; pm->restName = nm.text;
        } else {
          // keys: simple literals or names
          std::unique_ptr<ast::Expr> keyExpr;
          const auto& kt = get();
          if (kt.kind == TK::Ident || kt.kind == TK::TypeIdent) { keyExpr = std::make_unique<ast::Name>(kt.text); }
          else if (kt.kind == TK::String) { keyExpr = std::make_unique<ast::StringLiteral>(unquoteString(kt.text)); }
          else if (kt.kind == TK::Int) { keyExpr = std::make_unique<ast::IntLiteral>(std::stoll(kt.text)); }
          else if (kt.kind == TK::Float) { keyExpr = std::make_unique<ast::FloatLiteral>(std::stod(kt.text)); }
          else { throw std::runtime_error("Parse error: unsupported mapping key in pattern"); }
          expect(TK::Colon, ":'");
          auto pv = parsePattern();
          pm->items.emplace_back(std::move(keyExpr), std::move(pv));
        }
        first = false;
        if (peek().kind != TK::Comma) break;
      }
    }
    expect(TK::RBrace, "'}'");
    return pm;
  }
  // Class pattern: Name '(' [patterns] ')'
  if (tok.kind == TK::Ident && peek().kind == TK::LParen) {
    get(); // '('
    auto pc = std::make_unique<ast::PatternClass>(tok.text);
    if (peek().kind != TK::RParen) {
      bool seenKw = false;
      // Parse a sequence of positional patterns or keyword patterns name=pattern
      for (;;) {
        if (peek().kind == TK::Ident && peekNext().kind == TK::Equal) {
          // keyword pattern
          const auto nameTok = get(); get(); // '='
          pc->kwargs.emplace_back(nameTok.text, parsePattern());
          seenKw = true;
        } else {
          if (seenKw) { throw std::runtime_error("Parse error: positional pattern after keyword pattern"); }
          pc->args.emplace_back(parsePattern());
        }
        if (peek().kind != TK::Comma) break; get(); if (peek().kind == TK::RParen) break;
      }
    }
    expect(TK::RParen, "')'");
    return pc;
  }
  // Literal pattern: reuse primary literal parsing for ints/str/bool/float/None/bytes/ellipsis
  if (tok.kind == TK::Int || tok.kind == TK::Float || tok.kind == TK::String || tok.kind == TK::Bytes || tok.kind == TK::Ellipsis) {
    // Build same literal Expr as parsePrimary would
    std::unique_ptr<ast::Expr> lit;
    switch (tok.kind) {
      case TK::Int: lit = std::make_unique<ast::IntLiteral>(std::stoll(tok.text)); break;
      case TK::Float: lit = std::make_unique<ast::FloatLiteral>(std::stod(tok.text)); break;
      case TK::String: lit = std::make_unique<ast::StringLiteral>(unquoteString(tok.text)); break;
      case TK::Bytes: lit = std::make_unique<ast::BytesLiteral>(unquoteString(tok.text.substr(1))); break;
      case TK::Ellipsis: lit = std::make_unique<ast::EllipsisLiteral>(); break;
      default: break;
    }
    return std::make_unique<ast::PatternLiteral>(std::move(lit));
  }
  // None/True/False
  if (tok.kind == TK::TypeIdent && (tok.text == "None")) {
    return std::make_unique<ast::PatternLiteral>(std::make_unique<ast::NoneLiteral>());
  }
  if (tok.kind == TK::BoolLit) {
    const bool val = (tok.text == "True");
    return std::make_unique<ast::PatternLiteral>(std::make_unique<ast::BoolLiteral>(val));
  }
  throw std::runtime_error("Parse error: unsupported pattern");
}

std::unique_ptr<ast::Stmt> Parser::parseImportStmt() {
  if (peek().kind == TK::Import) {
    get(); auto imp = std::make_unique<ast::Import>();
    while (true) {
      // dotted name
      const auto& first = get(); if (first.kind != TK::Ident) break; std::string dotted = first.text;
      while (peek().kind == TK::Dot) {
        get(); const auto& nxt = get(); if (nxt.kind != TK::Ident) throw std::runtime_error("Parse error: expected ident after '.' in import");
        dotted += "."; dotted += nxt.text;
      }
      ast::Alias al{dotted,{}};
      if (peek().kind == TK::As) { get(); const auto& an = get(); if (an.kind == TK::Ident) al.asname = an.text; else throw std::runtime_error("Parse error: expected name after 'as'"); }
      imp->names.push_back(std::move(al));
      if (peek().kind != TK::Comma) break; get();
    }
    return imp;
  }
  // from import
  get();
  int level = 0;
  // Support both repeated '.' tokens and a single Ellipsis token ('...')
  while (peek().kind == TK::Dot || peek().kind == TK::Ellipsis) {
    if (peek().kind == TK::Dot) { get(); ++level; }
    else /* Ellipsis */ { get(); level += 3; }
  }
  std::string module;
  if (peek().kind == TK::Ident) {
    const auto& id = get(); module = id.text;
    while (peek().kind == TK::Dot) {
      get(); const auto& nxt = get(); if (nxt.kind != TK::Ident) throw std::runtime_error("Parse error: expected ident after '.' in from");
      module += "."; module += nxt.text;
    }
  }
  expect(TK::Import, "'import'"); auto imp = std::make_unique<ast::ImportFrom>(); imp->module = module; imp->level = level;
  if (peek().kind == TK::Star) { get(); return imp; }
  bool paren = false; if (peek().kind == TK::LParen) { paren = true; get(); }
  while (true) {
    const auto& nm = get(); if (nm.kind != TK::Ident) break; ast::Alias al{nm.text,{}};
    if (peek().kind == TK::As) { get(); const auto& an = get(); if (an.kind == TK::Ident) al.asname = an.text; else throw std::runtime_error("Parse error: expected name after 'as'"); }
    imp->names.push_back(std::move(al));
    if (peek().kind != TK::Comma) break; get(); if (paren && peek().kind == TK::RParen) break;
  }
  if (paren) expect(TK::RParen, ")'");
  return imp;
}

std::unique_ptr<ast::ClassDef> Parser::parseClass() {
  expect(TK::Class, "'class'"); const auto& nm = get(); if (nm.kind != TK::Ident) throw std::runtime_error("Parse error: expected class name");
  auto cls = std::make_unique<ast::ClassDef>(nm.text);
  if (peek().kind == TK::LParen) { get(); if (peek().kind != TK::RParen) { cls->bases.emplace_back(parseExpr()); while (peek().kind == TK::Comma) { get(); if (peek().kind == TK::RParen) break; cls->bases.emplace_back(parseExpr()); } } expect(TK::RParen, "')'"); }
  expect(TK::Colon, ":'"); if (peek().kind == TK::Newline) get(); expect(TK::Indent, "indent"); parseSuiteInto(cls->body); return cls;
}


ast::TypeKind Parser::toTypeKind(const std::string& typeName) {
  if (typeName == "int") { return ast::TypeKind::Int; }
  if (typeName == "bool") { return ast::TypeKind::Bool; }
  if (typeName == "float") { return ast::TypeKind::Float; }
  if (typeName == "str") { return ast::TypeKind::Str; }
  if (typeName == "None") { return ast::TypeKind::NoneType; }
  if (typeName == "tuple") { return ast::TypeKind::Tuple; }
  if (typeName == "list") { return ast::TypeKind::List; }
  if (typeName == "dict") { return ast::TypeKind::Dict; }
  if (typeName == "Optional") { return ast::TypeKind::Optional; }
  if (typeName == "Union") { return ast::TypeKind::Union; }
  return ast::TypeKind::NoneType;
}

} // namespace pycc::parse
