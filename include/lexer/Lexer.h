/***
 * Name: pycc::lex::Lexer
 * Purpose: Stream tokens from a stack of input sources (LIFO).
 * Inputs:
 *   - Initial file stream pushed via pushFile(); additional inputs can be
 *     pushed at any time and are processed LIFO.
 * Outputs:
 *   - A single token stream implementing ITokenStream. The parser pulls tokens
 *     on demand. Each token carries file, line, and column info.
 * Theory of Operation:
 *   Maintains a stack of per-source states. Scans line-by-line, emits INDENT/
 *   DEDENT/NEWLINE and tokens. Provides lookahead via internal buffering.
 */
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <istream>
#include <string>
#include <utility>
#include <vector>

namespace pycc::lex {

enum class TokenKind {
  End,        // EOF
  Newline,    // \n
  Indent,     // indentation increase
  Dedent,     // indentation decrease

  Def,        // def
  Return,     // return
  Del,        // del
  If,         // if
  Else,       // else
  Elif,       // elif
  While,      // while
  For,        // for
  In,         // in
  Break,      // break
  Continue,   // continue
  Pass,       // pass
  Try,        // try
  Except,     // except
  Finally,    // finally
  With,       // with
  As,         // as
  Match,      // match
  Case,       // case
  Import,     // import
  From,       // from
  Class,      // class
  At,         // @
  Assert,     // assert
  Raise,      // raise
  Global,     // global
  Nonlocal,   // nonlocal
  Yield,      // yield
  Await,      // await

  Arrow,      // ->
  Colon,      // :
  ColonEqual, // := (named expression)
  Comma,      // ,
  Equal,      // =
  PlusEqual,  // +=
  Plus,       // +
  MinusEqual, // -=
  Minus,      // -
  StarEqual,  // *=
  Star,       // *
  StarStarEqual, // **=
  StarStar,   // ** (power)
  SlashEqual, // /=
  Slash,      // /
  SlashSlashEqual, // //=
  SlashSlash, // // (floor-div)
  PercentEqual, // %=
  Percent,    // %
  LShiftEqual, // <<=
  LShift,     // <<
  RShiftEqual, // >>=
  RShift,     // >>
  AmpEqual,   // &=
  Amp,        // &
  CaretEqual, // ^=
  Caret,      // ^
  Tilde,      // ~
  PipeEqual,  // |=
  EqEq,       // ==
  NotEq,      // !=
  Lt,         // <
  Le,         // <=
  Gt,         // >
  Ge,         // >=
  Is,         // is
  And,        // and
  Or,         // or
  Not,        // not
  Lambda,     // lambda
  Pipe,       // |
  Dot,        // .
  LParen,     // (
  RParen,     // )
  LBracket,   // [
  RBracket,   // ]
  LBrace,     // {
  RBrace,     // }

  Ident,      // identifier
  Int,        // integer literal
  Float,      // float literal
  String,     // string literal
  BoolLit,    // True/False

  TypeIdent,  // type identifier (int, bool, float, str, None)
  Bytes,      // b'...'
  Ellipsis    // ...
};

inline const char* to_string(TokenKind k) {
  switch (k) {
    case TokenKind::End: return "End";
    case TokenKind::Newline: return "Newline";
    case TokenKind::Indent: return "Indent";
    case TokenKind::Dedent: return "Dedent";
    case TokenKind::Def: return "Def";
    case TokenKind::Return: return "Return";
    case TokenKind::Del: return "Del";
    case TokenKind::If: return "If";
    case TokenKind::Else: return "Else";
    case TokenKind::Elif: return "Elif";
    case TokenKind::While: return "While";
    case TokenKind::For: return "For";
    case TokenKind::In: return "In";
    case TokenKind::Break: return "Break";
    case TokenKind::Continue: return "Continue";
    case TokenKind::Pass: return "Pass";
    case TokenKind::Try: return "Try";
    case TokenKind::Except: return "Except";
    case TokenKind::Finally: return "Finally";
    case TokenKind::With: return "With";
    case TokenKind::As: return "As";
    case TokenKind::Match: return "Match";
    case TokenKind::Case: return "Case";
    case TokenKind::Import: return "Import";
    case TokenKind::From: return "From";
    case TokenKind::Class: return "Class";
    case TokenKind::At: return "At";
    case TokenKind::Assert: return "Assert";
    case TokenKind::Raise: return "Raise";
    case TokenKind::Global: return "Global";
    case TokenKind::Nonlocal: return "Nonlocal";
    case TokenKind::Yield: return "Yield";
    case TokenKind::Await: return "Await";
    case TokenKind::Arrow: return "Arrow";
    case TokenKind::Colon: return "Colon";
    case TokenKind::ColonEqual: return "ColonEqual";
    case TokenKind::Comma: return "Comma";
    case TokenKind::Equal: return "Equal";
    case TokenKind::PlusEqual: return "PlusEqual";
    case TokenKind::Plus: return "Plus";
    case TokenKind::MinusEqual: return "MinusEqual";
    case TokenKind::Minus: return "Minus";
    case TokenKind::StarEqual: return "StarEqual";
    case TokenKind::Star: return "Star";
    case TokenKind::StarStarEqual: return "StarStarEqual";
    case TokenKind::StarStar: return "StarStar";
    case TokenKind::SlashEqual: return "SlashEqual";
    case TokenKind::Slash: return "Slash";
    case TokenKind::SlashSlashEqual: return "SlashSlashEqual";
    case TokenKind::SlashSlash: return "SlashSlash";
    case TokenKind::PercentEqual: return "PercentEqual";
    case TokenKind::Percent: return "Percent";
    case TokenKind::LShiftEqual: return "LShiftEqual";
    case TokenKind::LShift: return "LShift";
    case TokenKind::RShiftEqual: return "RShiftEqual";
    case TokenKind::RShift: return "RShift";
    case TokenKind::AmpEqual: return "AmpEqual";
    case TokenKind::Amp: return "Amp";
    case TokenKind::CaretEqual: return "CaretEqual";
    case TokenKind::Caret: return "Caret";
    case TokenKind::Tilde: return "Tilde";
    case TokenKind::PipeEqual: return "PipeEqual";
    case TokenKind::EqEq: return "EqEq";
    case TokenKind::NotEq: return "NotEq";
    case TokenKind::Lt: return "Lt";
    case TokenKind::Le: return "Le";
    case TokenKind::Gt: return "Gt";
    case TokenKind::Ge: return "Ge";
    case TokenKind::Is: return "Is";
    case TokenKind::And: return "And";
    case TokenKind::Or: return "Or";
    case TokenKind::Not: return "Not";
    case TokenKind::Lambda: return "Lambda";
    case TokenKind::LParen: return "LParen";
    case TokenKind::RParen: return "RParen";
    case TokenKind::LBracket: return "LBracket";
    case TokenKind::RBracket: return "RBracket";
    case TokenKind::LBrace: return "LBrace";
    case TokenKind::RBrace: return "RBrace";
    case TokenKind::Pipe: return "Pipe";
    case TokenKind::Dot: return "Dot";
    case TokenKind::Ident: return "Ident";
    case TokenKind::Int: return "Int";
    case TokenKind::Float: return "Float";
    case TokenKind::String: return "String";
    case TokenKind::BoolLit: return "BoolLit";
    case TokenKind::TypeIdent: return "TypeIdent";
    case TokenKind::Bytes: return "Bytes";
    case TokenKind::Ellipsis: return "Ellipsis";
  }
  return "Unknown";
}

struct Token {
  TokenKind kind{TokenKind::End};
  std::string text{}; // original text (identifier/int/typename)
  std::string file{};
  int line{1};        // 1-based line number
  int col{1};         // 1-based column at token start
};

// Abstract token stream interface
class ITokenStream {
 public:
  virtual ~ITokenStream() = default;
  virtual const Token& peek(size_t k = 0) = 0; // lookahead k (0=current)
  virtual Token next() = 0;                     // consume next token
};

// Input source abstraction
class InputSource {
 public:
  virtual ~InputSource() = default;
  virtual bool getline(std::string& out) = 0; // returns false on EOF
  virtual const std::string& name() const = 0;
};

// File-backed input
class FileInput : public InputSource {
 public:
  explicit FileInput(std::string path);
  bool getline(std::string& out) override;
  const std::string& name() const override { return path_; }
 private:
  std::string path_{};
  std::unique_ptr<std::istream> in_{nullptr};
};

// String-backed input (useful for tests)
class StringInput : public InputSource {
 public:
  StringInput(std::string text, std::string name);
  bool getline(std::string& out) override;
  const std::string& name() const override { return name_; }
 private:
  std::string name_{};
  std::unique_ptr<std::istream> in_{nullptr};
};

class Lexer : public ITokenStream {
 public:
  Lexer() = default;
  void pushFile(const std::string& path);
  void pushString(const std::string& text, const std::string& name);

  // ITokenStream
  const Token& peek(size_t lookahead = 0) override;
  Token next() override;
  std::vector<Token> tokens();

 private:
  // Eager tokenization buffer to simplify streaming semantics safely
  bool finalized_{false};
  std::vector<Token> tokens_{};
  size_t pos_{0};

  struct State {
    std::unique_ptr<InputSource> src;
    std::string line;
    size_t index{0};
    int lineNo{0};
    std::vector<size_t> indentStack{0};
    bool needIndentCheck{true};
    bool pendingNewline{false};
  };

  std::vector<State> stack_{};   // LIFO of inputs
  std::deque<Token> buffer_{};   // lookahead buffer

  // helpers
  bool ensure(size_t lookahead);            // ensure buffer has at least k+1 tokens
  bool refill();                    // append next token to buffer
  bool readNextLine(State& st);     // load next line into state
  bool atEOF() const;               // no more inputs
  bool emitIndentTokens(State& state, std::vector<Token>& out, int baseCol);
  Token scanOne(State& state);      // scan a single token from current state

  void buildAll();                  // build tokens_ from all inputs (LIFO)
};

} // namespace pycc::lex
