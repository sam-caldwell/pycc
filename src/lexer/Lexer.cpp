/***
 * Name: pycc::lex::Lexer (streaming)
 * Purpose: Tokenize source(s) into a single token stream (LIFO inputs).
 */
#include "lexer/Lexer.h"
#include <cctype>
#include <fstream>
#include <sstream>

namespace pycc::lex {

static bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
static bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// FileInput implementation
FileInput::FileInput(std::string path) : path_(std::move(path)) {
  auto ifs = std::make_unique<std::ifstream>(path_);
  in_ = std::move(ifs);
}
bool FileInput::getline(std::string& out) {
  if (!in_ || !(*in_)) return false;
  if (!std::getline(*in_, out)) return false;
  return true;
}

// StringInput implementation
StringInput::StringInput(std::string text, std::string name)
  : name_(std::move(name)) {
  auto iss = std::make_unique<std::istringstream>(std::move(text));
  in_ = std::move(iss);
}
bool StringInput::getline(std::string& out) {
  if (!in_ || !(*in_)) return false;
  if (!std::getline(*in_, out)) return false;
  return true;
}

void Lexer::pushFile(const std::string& path) {
  State st; st.src = std::make_unique<FileInput>(path); st.lineNo = 0; st.index = 0; st.indentStack = {0}; st.needIndentCheck = true; st.pendingNewline = false;
  stack_.push_back(std::move(st));
}

void Lexer::pushString(const std::string& text, const std::string& name) {
  State st; st.src = std::make_unique<StringInput>(text, name); st.lineNo = 0; st.index = 0; st.indentStack = {0}; st.needIndentCheck = true; st.pendingNewline = false;
  stack_.push_back(std::move(st));
}

bool Lexer::readNextLine(State& st) {
  st.line.clear();
  if (!st.src->getline(st.line)) return false;
  ++st.lineNo;
  st.index = 0;
  // Handle CRLF
  if (!st.line.empty() && st.line.back() == '\r') st.line.pop_back();
  st.needIndentCheck = true;
  st.pendingNewline = false;
  return true;
}

bool Lexer::emitIndentTokens(State& st, std::vector<Token>& out, const int baseCol) {
  // Compute leading spaces
  size_t i = 0; size_t spaces = 0;
  while (i < st.line.size() && st.line[i] == ' ') { ++i; ++spaces; }
  // Empty or comment line: emit Newline and skip indent changes
  bool allSpace = (i >= st.line.size());
  bool comment = (!allSpace && st.line[i] == '#');
  if (allSpace || comment) {
    Token t; t.kind = TokenKind::Newline; t.text = "\n"; t.file = st.src->name(); t.line = st.lineNo; t.col = 1;
    out.push_back(std::move(t));
    st.index = st.line.size();
    st.needIndentCheck = false;
    st.pendingNewline = false;
    return true; // handled
  }
  if (st.needIndentCheck) {
    if (spaces > st.indentStack.back()) {
      st.indentStack.push_back(spaces);
      Token t; t.kind = TokenKind::Indent; t.text = "<INDENT>"; t.file = st.src->name(); t.line = st.lineNo; t.col = baseCol;
      out.push_back(std::move(t));
    } else {
      while (spaces < st.indentStack.back()) {
        st.indentStack.pop_back();
        Token t; t.kind = TokenKind::Dedent; t.text = "<DEDENT>"; t.file = st.src->name(); t.line = st.lineNo; t.col = baseCol;
        out.push_back(std::move(t));
      }
    }
    st.index = i; // start scanning after indent
    st.needIndentCheck = false;
  }
  return false;
}

Token Lexer::scanOne(State& st) {
  auto& line = st.line;
  size_t& i = st.index;

  if (i >= line.size()) {
    st.pendingNewline = false;
    Token t; t.kind = TokenKind::Newline; t.text = "\n"; t.file = st.src->name(); t.line = st.lineNo; t.col = static_cast<int>(line.size() + 1);
    return t;
  }

  auto makeTok = [&](TokenKind k, size_t start, size_t endExclusive) {
    Token t; t.kind = k; t.text = line.substr(start, endExclusive - start); t.file = st.src->name(); t.line = st.lineNo; t.col = static_cast<int>(start + 1);
    return t;
  };

  char c = line[i];
  if (c == ' ' || c == '\t') { ++i; return scanOne(st); }
  if (c == '#') { st.index = line.size(); Token t; t.kind = TokenKind::Newline; t.text = "\n"; t.file = st.src->name(); t.line = st.lineNo; t.col = static_cast<int>(line.size() + 1); return t; }
  if (c == '(') { ++i; return makeTok(TokenKind::LParen, i-1, i); }
  if (c == ')') { ++i; return makeTok(TokenKind::RParen, i-1, i); }
  if (c == '[') { ++i; return makeTok(TokenKind::LBracket, i-1, i); }
  if (c == ']') { ++i; return makeTok(TokenKind::RBracket, i-1, i); }
  if (c == ':') { ++i; return makeTok(TokenKind::Colon, i-1, i); }
  if (c == ',') { ++i; return makeTok(TokenKind::Comma, i-1, i); }
  if (c == '+') { ++i; return makeTok(TokenKind::Plus, i-1, i); }
  if (c == '"' || c == '\'') {
    char quote = c; size_t j = i + 1;
    while (j < line.size() && line[j] != quote) { ++j; }
    // consume closing quote if present
    size_t end = (j < line.size() && line[j] == quote) ? (j + 1) : j;
    Token t = makeTok(TokenKind::String, i, end);
    i = end;
    return t;
  }
  if (c == '-') {
    if (i + 1 < line.size() && line[i+1] == '>') { i += 2; return makeTok(TokenKind::Arrow, i-2, i); }
    ++i; return makeTok(TokenKind::Minus, i-1, i);
  }
  if (c == '*') { ++i; return makeTok(TokenKind::Star, i-1, i); }
  if (c == '/') { ++i; return makeTok(TokenKind::Slash, i-1, i); }
  if (c == '%') { ++i; return makeTok(TokenKind::Percent, i-1, i); }
  if (c == '=') { if (i + 1 < line.size() && line[i+1] == '=') { i += 2; return makeTok(TokenKind::EqEq, i-2, i); } ++i; return makeTok(TokenKind::Equal, i-1, i); }
  if (c == '!') { if (i + 1 < line.size() && line[i+1] == '=') { i += 2; return makeTok(TokenKind::NotEq, i-2, i); } }
  if (c == '<') { if (i + 1 < line.size() && line[i+1] == '=') { i += 2; return makeTok(TokenKind::Le, i-2, i); } ++i; return makeTok(TokenKind::Lt, i-1, i); }
  if (c == '>') { if (i + 1 < line.size() && line[i+1] == '=') { i += 2; return makeTok(TokenKind::Ge, i-2, i); } ++i; return makeTok(TokenKind::Gt, i-1, i); }
  if (c == '|') { ++i; return makeTok(TokenKind::Pipe, i-1, i); }

  auto scanExponent = [&](size_t pos) -> size_t {
    size_t k = pos;
    if (k < line.size() && (line[k] == 'e' || line[k] == 'E')) {
      ++k;
      if (k < line.size() && (line[k] == '+' || line[k] == '-')) ++k;
      size_t start = k;
      while (k < line.size() && std::isdigit(static_cast<unsigned char>(line[k]))) ++k;
      if (k == start) return pos; // back out if no digits
      return k;
    }
    return pos;
  };

  if (std::isdigit(static_cast<unsigned char>(c))) {
    size_t j = i;
    while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j]))) ++j;
    size_t k = j;
    if (k < line.size() && line[k] == '.') {
      size_t m = k + 1;
      if (m < line.size() && std::isdigit(static_cast<unsigned char>(line[m]))) {
        while (m < line.size() && std::isdigit(static_cast<unsigned char>(line[m]))) ++m;
        size_t epos = scanExponent(m);
        Token t = makeTok(TokenKind::Float, i, epos); i = epos; return t;
      } else {
        size_t epos = scanExponent(k + 1);
        Token t = makeTok(TokenKind::Float, i, epos); i = epos; return t;
      }
    }
    size_t epos = scanExponent(k);
    if (epos != k) { Token t = makeTok(TokenKind::Float, i, epos); i = epos; return t; }
    Token t = makeTok(TokenKind::Int, i, j); i = j; return t;
  }
  if (c == '.' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i+1]))) {
    size_t m = i + 1; while (m < line.size() && std::isdigit(static_cast<unsigned char>(line[m]))) ++m; size_t epos = scanExponent(m); Token t = makeTok(TokenKind::Float, i, epos); i = epos; return t;
  }

  if (isIdentStart(c)) {
    size_t j = i + 1; while (j < line.size() && isIdentChar(line[j])) ++j;
    std::string ident = line.substr(i, j - i);
    TokenKind k = TokenKind::Ident;
    if (ident == "def") k = TokenKind::Def;
    else if (ident == "return") k = TokenKind::Return;
    else if (ident == "if") k = TokenKind::If;
    else if (ident == "else") k = TokenKind::Else;
    else if (ident == "and") k = TokenKind::And;
    else if (ident == "or") k = TokenKind::Or;
    else if (ident == "not") k = TokenKind::Not;
    else if (ident == "True" || ident == "False") k = TokenKind::BoolLit;
    else if (ident == "int" || ident == "bool" || ident == "float" || ident == "str" || ident == "None" || ident == "tuple" || ident == "list" || ident == "dict") k = TokenKind::TypeIdent;
    Token t = makeTok(k, i, j); i = j; return t;
  }

  // Unknown/other: skip char
  ++i; return scanOne(st);
}

bool Lexer::atEOF() const { return stack_.empty(); }

bool Lexer::refill() {
  while (true) {
    if (atEOF()) return false;
    State& st = stack_.back();
    // Handle end-of-line and new lines
    while (st.index >= st.line.size()) {
      if (st.pendingNewline) {
        st.pendingNewline = false;
        Token t; t.kind = TokenKind::Newline; t.text = "\n"; t.file = st.src->name(); t.line = st.lineNo; t.col = static_cast<int>(st.line.size() + 1);
        buffer_.push_back(t); return true;
      }
      if (!readNextLine(st)) {
        if (st.indentStack.size() > 1) {
          st.indentStack.pop_back();
          Token t; t.kind = TokenKind::Dedent; t.text = "<DEDENT>"; t.file = st.src->name(); t.line = st.lineNo + 1; t.col = 1;
          buffer_.push_back(t); return true;
        }
        // done with this source
        stack_.pop_back();
        continue; // switch to next top
      }
      std::vector<Token> inds;
      if (emitIndentTokens(st, inds, 1)) { buffer_.push_back(inds.front()); return true; }
      if (!inds.empty()) { buffer_.push_back(inds.front()); for (size_t i = 1; i < inds.size(); ++i) buffer_.push_back(inds[i]); return true; }
    }
    // We have characters to scan
    Token t = scanOne(st);
    if (st.index >= st.line.size()) st.pendingNewline = true;
    buffer_.push_back(t);
    return true;
  }
}

bool Lexer::ensure(size_t k) {
  while (buffer_.size() <= k) {
    if (!refill()) break;
  }
  return buffer_.size() > k;
}

void Lexer::buildAll() {
  if (finalized_) return;
  finalized_ = true;
  // Process stack in LIFO order
  while (!stack_.empty()) {
    State st = std::move(stack_.back());
    stack_.pop_back();
    st.indentStack = {0}; st.line.clear(); st.index = 0; st.lineNo = 0; st.needIndentCheck = true; st.pendingNewline = false;
    // Read each line
    while (readNextLine(st)) {
      // Handle indent/dedent and skip empty/comment lines
      std::vector<Token> indents;
      if (emitIndentTokens(st, indents, 1)) {
        tokens_.push_back(indents.front());
        continue;
      }
      for (auto& t : indents) tokens_.push_back(t);

      // Tokenize rest of the line
      while (st.index < st.line.size()) {
        Token t = scanOne(st);
        // scanOne may return newline for comments/empty trailing
        if (t.kind == TokenKind::Newline) { tokens_.push_back(t); break; }
        tokens_.push_back(t);
      }
      if (st.index >= st.line.size()) {
        Token nl; nl.kind = TokenKind::Newline; nl.text = "\n"; nl.file = st.src->name(); nl.line = st.lineNo; nl.col = static_cast<int>(st.line.size() + 1);
        tokens_.push_back(nl);
      }
    }
    // flush dedents
    while (st.indentStack.size() > 1) {
      st.indentStack.pop_back();
      Token t; t.kind = TokenKind::Dedent; t.text = "<DEDENT>"; t.file = st.src->name(); t.line = st.lineNo + 1; t.col = 1;
      tokens_.push_back(t);
    }
  }
  // Final EOF
  Token eof; eof.kind = TokenKind::End; eof.text = "<EOF>"; eof.file = ""; eof.line = 0; eof.col = 1; tokens_.push_back(eof);
}

const Token& Lexer::peek(size_t k) {
  if (!finalized_) buildAll();
  if (pos_ + k < tokens_.size()) return tokens_[pos_ + k];
  return tokens_.back();
}

Token Lexer::next() {
  if (!finalized_) buildAll();
  if (pos_ < tokens_.size()) return tokens_[pos_++];
  return tokens_.back();
}

std::vector<Token> Lexer::tokens() {
  if (!finalized_) buildAll();
  return tokens_;
}

} // namespace pycc::lex
