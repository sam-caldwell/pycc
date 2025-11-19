/***
 * Name: pycc::lex::Lexer (streaming)
 * Purpose: Tokenize source(s) into a single token stream (LIFO inputs).
 */
#include "lexer/Lexer.h"
#include <cctype>
#include <cstddef>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace pycc::lex {

static bool isIdentStart(char chr) { return (std::isalpha(static_cast<unsigned char>(chr)) != 0) || chr == '_'; }
static bool isIdentChar(char chr) { return (std::isalnum(static_cast<unsigned char>(chr)) != 0) || chr == '_'; }

// FileInput implementation
FileInput::FileInput(std::string path) : path_(std::move(path)), in_(nullptr) {
  auto ifs = std::make_unique<std::ifstream>(path_);
  in_ = std::move(ifs);
}

bool FileInput::getline(std::string& out) {
  if (!in_ || !(*in_)) { return false; }
  if (!std::getline(*in_, out)) { return false; }
  return true;
}

// StringInput implementation
StringInput::StringInput(std::string text, std::string name)
  : name_(std::move(name)), in_(nullptr) {
  auto iss = std::make_unique<std::istringstream>(std::move(text));
  in_ = std::move(iss);
}

bool StringInput::getline(std::string& out) {
  if (!in_ || !(*in_)) { return false; }
  if (!std::getline(*in_, out)) { return false; }
  return true;
}


void Lexer::pushFile(const std::string& path) {
  State state;
  state.src = std::make_unique<FileInput>(path);
  state.lineNo = 0;
  state.index = 0;
  state.indentStack = {0};
  state.needIndentCheck = true;
  state.pendingNewline = false;
  stack_.push_back(std::move(state));
}


void Lexer::pushString(const std::string& text, const std::string& name) {
  State state;
  state.src = std::make_unique<StringInput>(text, name);
  state.lineNo = 0;
  state.index = 0;
  state.indentStack = {0};
  state.needIndentCheck = true;
  state.pendingNewline = false;
  stack_.push_back(std::move(state));
}


bool Lexer::readNextLine(State& state) {
  state.line.clear();
  if (!state.src->getline(state.line)) { return false; }
  ++state.lineNo;
  state.index = 0;
  // Handle CRLF
  if (!state.line.empty() && state.line.back() == '\r') { state.line.pop_back(); }
  state.needIndentCheck = true;
  state.pendingNewline = false;
  return true;
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
bool Lexer::emitIndentTokens(State& state, std::vector<Token>& out, const int baseCol) {
  // Compute leading spaces
  size_t idx = 0; size_t spaces = 0;
  while (idx < state.line.size() && state.line[idx] == ' ') { ++idx; ++spaces; }
  // Empty or comment line: emit Newline and skip indent changes
  const bool allSpace = (idx >= state.line.size());
  const bool comment = (!allSpace && state.line[idx] == '#');
  if (allSpace || comment) {
    Token tok; tok.kind = TokenKind::Newline; tok.text = "\n"; tok.file = state.src->name(); tok.line = state.lineNo; tok.col = 1;
    out.push_back(std::move(tok));
    state.index = state.line.size();
    state.needIndentCheck = false;
    state.pendingNewline = false;
    return true; // handled
  }
  if (state.needIndentCheck) {
    if (spaces > state.indentStack.back()) {
      state.indentStack.push_back(spaces);
      Token tok; tok.kind = TokenKind::Indent; tok.text = "<INDENT>"; tok.file = state.src->name(); tok.line = state.lineNo; tok.col = baseCol;
      out.push_back(std::move(tok));
    } else {
      while (spaces < state.indentStack.back()) {
        state.indentStack.pop_back();
        Token tok; tok.kind = TokenKind::Dedent; tok.text = "<DEDENT>"; tok.file = state.src->name(); tok.line = state.lineNo; tok.col = baseCol;
        out.push_back(std::move(tok));
      }
    }
    state.index = idx; // start scanning after indent
    state.needIndentCheck = false;
  }
  return false;
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
Token Lexer::scanOne(State& state) {
  auto& line = state.line;
  size_t& idx = state.index;

  if (idx >= line.size()) {
    state.pendingNewline = false;
    Token newlineTok; newlineTok.kind = TokenKind::Newline; newlineTok.text = "\n"; newlineTok.file = state.src->name(); newlineTok.line = state.lineNo; newlineTok.col = static_cast<int>(line.size() + 1);
    return newlineTok;
  }

  auto makeTok = [&](TokenKind kind, size_t start, size_t endExclusive) {
    Token tok;
    tok.kind = kind;
    tok.text = line.substr(start, endExclusive - start);
    tok.file = state.src->name();
    tok.line = state.lineNo;
    tok.col = static_cast<int>(start + 1);
    return tok;
  };
  // Inline comment: treat rest of line as comment and emit newline
  if (line[idx] == '#') {
    idx = line.size();
    Token newlineTok; newlineTok.kind = TokenKind::Newline; newlineTok.text = "\n"; newlineTok.file = state.src->name(); newlineTok.line = state.lineNo; newlineTok.col = static_cast<int>(line.size() + 1);
    return newlineTok;
  }

  const char chr = line[idx];
  if (chr == ' ' || chr == '\t') { ++idx; return scanOne(state); }
  if (chr == '#') {
    state.index = line.size();
    Token tok;
    tok.kind = TokenKind::Newline;
    tok.text = "\n";
    tok.file = state.src->name();
    tok.line = state.lineNo;
    tok.col = static_cast<int>(line.size() + 1);
    return tok;
  }
  if (chr == '(') { ++idx; return makeTok(TokenKind::LParen, idx-1, idx); }
  if (chr == ')') { ++idx; return makeTok(TokenKind::RParen, idx-1, idx); }
  if (chr == '[') { ++idx; return makeTok(TokenKind::LBracket, idx-1, idx); }
  if (chr == ']') { ++idx; return makeTok(TokenKind::RBracket, idx-1, idx); }
  if (chr == '{') { ++idx; return makeTok(TokenKind::LBrace, idx-1, idx); }
  if (chr == '}') { ++idx; return makeTok(TokenKind::RBrace, idx-1, idx); }
  if (chr == ':') {
    if (idx + 1 < line.size() && line[idx+1] == '=') {
      idx += 2; return makeTok(TokenKind::ColonEqual, idx-2, idx);
    }
    ++idx; return makeTok(TokenKind::Colon, idx-1, idx);
  }
  if (chr == ',') { ++idx; return makeTok(TokenKind::Comma, idx-1, idx); }
  if (chr == '@') { ++idx; return makeTok(TokenKind::At, idx-1, idx); }
  if (chr == '+') {
    if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::PlusEqual, idx-2, idx); }
    ++idx; return makeTok(TokenKind::Plus, idx-1, idx);
  }
  auto scanStringLike = [&](size_t start, bool bytesPrefix) -> Token {
    const char first = line[start];
    size_t p = start;
    // gather optional second prefix (f/r/b) already accounted outside; if prefix char then move one
    if ((first=='b'||first=='B'||first=='f'||first=='F'||first=='r'||first=='R') && p+1 < line.size()) { ++p; }
    const char quote = line[p]; bool triple = false;
    if (p+2 < line.size() && line[p]==line[p+1] && line[p]==line[p+2] && (line[p]=='"'||line[p]=='\'')) { triple = true; }
    // advance idx conservatively to end of current line token extent
    size_t endPos;
    if (!triple) {
      size_t j = p+1; while (j < line.size() && line[j] != quote) ++j; endPos = (j < line.size() ? j+1 : j);
    } else {
      // consume triple opener and pretend it ends immediately (placeholder)
      endPos = p + 3; // keep parser progressing; actual content spans multiple lines
    }
    Token tok = makeTok(bytesPrefix ? TokenKind::Bytes : TokenKind::String, start, endPos);
    if (!triple) {
      // For single-line strings, keep the real lexeme text (including quotes) for unquoting later
      tok.text = line.substr(start, endPos - start);
    } else {
      // Triple-quoted placeholder to avoid multi-line scan here
      const std::string pref = line.substr(start, p - start);
      const std::string q = std::string(3, line[p]);
      tok.text = pref + q + "..." + q;
    }
    idx = endPos;
    return tok;
  };
  if (chr == '"' || chr == '\'') { return scanStringLike(idx, /*bytesPrefix=*/false); }
  // prefixes: b/B, f/F, r/R and combos like fr/rf -> treat as String, Bytes if b present
  if ((chr=='b'||chr=='B'||chr=='f'||chr=='F'||chr=='r'||chr=='R') && idx+1 < line.size()) {
    size_t p = idx; bool hasB=false; int cnt=0;
    while (cnt<2 && p<line.size() && (line[p]=='b'||line[p]=='B'||line[p]=='f'||line[p]=='F'||line[p]=='r'||line[p]=='R')) { hasB = hasB || (line[p]=='b'||line[p]=='B'); ++p; ++cnt; }
    if (p < line.size() && (line[p]=='\''||line[p]=='"')) { return scanStringLike(idx, hasB); }
    if (p+2 < line.size() && line[p]==line[p+1] && line[p]==line[p+2] && (line[p]=='\''||line[p]=='"')) { return scanStringLike(idx, hasB); }
  }
  if (chr == '-') {
    if (idx + 1 < line.size() && line[idx+1] == '>') { idx += 2; return makeTok(TokenKind::Arrow, idx-2, idx); }
    if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::MinusEqual, idx-2, idx); }
    ++idx; return makeTok(TokenKind::Minus, idx-1, idx);
  }
  if (chr == '*') {
    if (idx + 1 < line.size() && line[idx+1] == '*') {
      if (idx + 2 < line.size() && line[idx+2] == '=') { idx += 3; return makeTok(TokenKind::StarStarEqual, idx-3, idx); }
      idx += 2; return makeTok(TokenKind::StarStar, idx-2, idx);
    }
    if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::StarEqual, idx-2, idx); }
    ++idx; return makeTok(TokenKind::Star, idx-1, idx);
  }
  if (chr == '/') {
    if (idx + 1 < line.size() && line[idx+1] == '/') {
      if (idx + 2 < line.size() && line[idx+2] == '=') { idx += 3; return makeTok(TokenKind::SlashSlashEqual, idx-3, idx); }
      idx += 2; return makeTok(TokenKind::SlashSlash, idx-2, idx);
    }
    if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::SlashEqual, idx-2, idx); }
    ++idx; return makeTok(TokenKind::Slash, idx-1, idx);
  }
  if (chr == '%') { if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::PercentEqual, idx-2, idx); } ++idx; return makeTok(TokenKind::Percent, idx-1, idx); }
  if (chr == '=') { if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::EqEq, idx-2, idx); } ++idx; return makeTok(TokenKind::Equal, idx-1, idx); }
  if (chr == '!') { if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::NotEq, idx-2, idx); } }
  if (chr == '<') {
    if (idx + 1 < line.size() && line[idx+1] == '<') {
      if (idx + 2 < line.size() && line[idx+2] == '=') { idx += 3; return makeTok(TokenKind::LShiftEqual, idx-3, idx); }
      idx += 2; return makeTok(TokenKind::LShift, idx-2, idx);
    }
    if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::Le, idx-2, idx); }
    ++idx; return makeTok(TokenKind::Lt, idx-1, idx);
  }
  if (chr == '>') {
    if (idx + 1 < line.size() && line[idx+1] == '>') {
      if (idx + 2 < line.size() && line[idx+2] == '=') { idx += 3; return makeTok(TokenKind::RShiftEqual, idx-3, idx); }
      idx += 2; return makeTok(TokenKind::RShift, idx-2, idx);
    }
    if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::Ge, idx-2, idx); }
    ++idx; return makeTok(TokenKind::Gt, idx-1, idx);
  }
  if (chr == '|') { if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::PipeEqual, idx-2, idx); } ++idx; return makeTok(TokenKind::Pipe, idx-1, idx); }
  if (chr == '&') { if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::AmpEqual, idx-2, idx); } ++idx; return makeTok(TokenKind::Amp, idx-1, idx); }
  if (chr == '^') { if (idx + 1 < line.size() && line[idx+1] == '=') { idx += 2; return makeTok(TokenKind::CaretEqual, idx-2, idx); } ++idx; return makeTok(TokenKind::Caret, idx-1, idx); }
  if (chr == '~') { ++idx; return makeTok(TokenKind::Tilde, idx-1, idx); }

  auto scanExponent = [&](size_t pos) -> size_t {
    size_t idx = pos;
    if (idx < line.size() && (line[idx] == 'e' || line[idx] == 'E')) {
      ++idx;
      if (idx < line.size() && (line[idx] == '+' || line[idx] == '-')) { ++idx; }
      const size_t startIdx = idx;
      while (idx < line.size() && std::isdigit(static_cast<unsigned char>(line[idx])) != 0) { ++idx; }
      if (idx == startIdx) { return pos; } // back out if no digits
      return idx;
    }
    return pos;
  };

  if (std::isdigit(static_cast<unsigned char>(chr)) != 0) {
    size_t jpos = idx;
    while (jpos < line.size() && std::isdigit(static_cast<unsigned char>(line[jpos])) != 0) { ++jpos; }
    const size_t kpos = jpos;
    if (kpos < line.size() && line[kpos] == '.') {
      size_t mpos = kpos + 1;
      if (mpos < line.size() && std::isdigit(static_cast<unsigned char>(line[mpos])) != 0) {
        while (mpos < line.size() && std::isdigit(static_cast<unsigned char>(line[mpos])) != 0) { ++mpos; }
        const size_t epos = scanExponent(mpos);
        size_t end = epos;
        if (end < line.size() && (line[end] == 'j' || line[end] == 'J')) { ++end; const Token tok = makeTok(TokenKind::Imag, idx, end); idx = end; return tok; }
        const Token tok = makeTok(TokenKind::Float, idx, epos); idx = epos; return tok;
      }
      const size_t epos = scanExponent(kpos + 1);
      size_t end = epos;
      if (end < line.size() && (line[end] == 'j' || line[end] == 'J')) { ++end; const Token tok = makeTok(TokenKind::Imag, idx, end); idx = end; return tok; }
      const Token tok = makeTok(TokenKind::Float, idx, epos); idx = epos; return tok;
    }
    const size_t epos = scanExponent(kpos);
    if (epos != kpos) {
      size_t end = epos;
      if (end < line.size() && (line[end] == 'j' || line[end] == 'J')) { ++end; const Token tok = makeTok(TokenKind::Imag, idx, end); idx = end; return tok; }
      const Token tok = makeTok(TokenKind::Float, idx, epos); idx = epos; return tok;
    }
    size_t end = jpos;
    if (end < line.size() && (line[end] == 'j' || line[end] == 'J')) { ++end; const Token tok = makeTok(TokenKind::Imag, idx, end); idx = end; return tok; }
    const Token tok = makeTok(TokenKind::Int, idx, jpos); idx = jpos; return tok;
  }
  if (chr == '.') {
    if (idx + 2 < line.size() && line[idx+1] == '.' && line[idx+2] == '.') { idx += 3; return makeTok(TokenKind::Ellipsis, idx-3, idx); }
    if (idx + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[idx+1])) != 0) {
      size_t mpos = idx + 1;
      while (mpos < line.size() && std::isdigit(static_cast<unsigned char>(line[mpos])) != 0) { ++mpos; }
      const size_t epos = scanExponent(mpos);
      size_t end = epos;
      if (end < line.size() && (line[end] == 'j' || line[end] == 'J')) { ++end; const Token tok = makeTok(TokenKind::Imag, idx, end); idx = end; return tok; }
      const Token tok = makeTok(TokenKind::Float, idx, epos); idx = epos; return tok;
    }
    ++idx; return makeTok(TokenKind::Dot, idx-1, idx);
  }

  if (isIdentStart(chr)) {
    size_t jpos = idx + 1;
    while (jpos < line.size() && isIdentChar(line[jpos])) { ++jpos; }
    const std::string ident = line.substr(idx, jpos - idx);
    TokenKind kind = TokenKind::Ident;
    if (ident == "def") { kind = TokenKind::Def; }
    else if (ident == "return") { kind = TokenKind::Return; }
    else if (ident == "del") { kind = TokenKind::Del; }
    else if (ident == "if") { kind = TokenKind::If; }
    else if (ident == "else") { kind = TokenKind::Else; }
    else if (ident == "elif") { kind = TokenKind::Elif; }
    else if (ident == "while") { kind = TokenKind::While; }
    else if (ident == "for") { kind = TokenKind::For; }
    else if (ident == "in") { kind = TokenKind::In; }
    else if (ident == "break") { kind = TokenKind::Break; }
    else if (ident == "continue") { kind = TokenKind::Continue; }
    else if (ident == "pass") { kind = TokenKind::Pass; }
    else if (ident == "try") { kind = TokenKind::Try; }
    else if (ident == "except") { kind = TokenKind::Except; }
    else if (ident == "finally") { kind = TokenKind::Finally; }
    else if (ident == "with") { kind = TokenKind::With; }
    else if (ident == "as") { kind = TokenKind::As; }
    else if (ident == "match") { kind = TokenKind::Match; }
    else if (ident == "case") { kind = TokenKind::Case; }
    else if (ident == "import") { kind = TokenKind::Import; }
    else if (ident == "from") { kind = TokenKind::From; }
    else if (ident == "class") { kind = TokenKind::Class; }
    else if (ident == "async") { kind = TokenKind::Async; }
    else if (ident == "assert") { kind = TokenKind::Assert; }
    else if (ident == "raise") { kind = TokenKind::Raise; }
    else if (ident == "global") { kind = TokenKind::Global; }
    else if (ident == "nonlocal") { kind = TokenKind::Nonlocal; }
    else if (ident == "yield") { kind = TokenKind::Yield; }
    else if (ident == "await") { kind = TokenKind::Await; }
    else if (ident == "and") { kind = TokenKind::And; }
    else if (ident == "or") { kind = TokenKind::Or; }
    else if (ident == "not") { kind = TokenKind::Not; }
    else if (ident == "lambda") { kind = TokenKind::Lambda; }
    else if (ident == "is") { kind = TokenKind::Is; }
    else if (ident == "True" || ident == "False") { kind = TokenKind::BoolLit; }
    else if (ident == "int" || ident == "bool" || ident == "float" || ident == "str" || ident == "None" || ident == "tuple" || ident == "list" || ident == "dict" || ident == "Optional" || ident == "Union") { kind = TokenKind::TypeIdent; }
    const Token tok = makeTok(kind, idx, jpos);
    idx = jpos;
    return tok;
  }

  // Unknown/other: skip char
  ++idx; return scanOne(state);
}

bool Lexer::atEOF() const { return stack_.empty(); }

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
bool Lexer::refill() {
  while (true) {
    if (atEOF()) {
      return false;
    }

    State& state = stack_.back();

    // Handle end-of-line and new lines (flattened to reduce nesting)
    if (state.index >= state.line.size()) {
      if (state.pendingNewline) {
        state.pendingNewline = false;
        Token newlineTok;
        newlineTok.kind = TokenKind::Newline;
        newlineTok.text = "\n";
        newlineTok.file = state.src->name();
        newlineTok.line = state.lineNo;
        newlineTok.col = static_cast<int>(state.line.size() + 1);
        buffer_.push_back(newlineTok);
        return true;
      }

      if (!readNextLine(state)) {
        if (state.indentStack.size() > 1) {
          state.indentStack.pop_back();
          Token dedentTok;
          dedentTok.kind = TokenKind::Dedent;
          dedentTok.text = "<DEDENT>";
          dedentTok.file = state.src->name();
          dedentTok.line = state.lineNo + 1;
          dedentTok.col = 1;
          buffer_.push_back(dedentTok);
          return true;
        }
        // done with this source; switch to next top on next loop
        stack_.pop_back();
        continue;
      }

      std::vector<Token> indentTokens;
      if (emitIndentTokens(state, indentTokens, 1)) {
        buffer_.push_back(indentTokens.front());
        return true;
      }
      if (!indentTokens.empty()) {
        buffer_.push_back(indentTokens.front());
        for (size_t idx = 1; idx < indentTokens.size(); ++idx) {
          buffer_.push_back(indentTokens[idx]);
        }
        return true;
      }
      // Fallthrough: have content on this line; proceed to scanning below
    }

    // We have characters to scan
    const Token tok = scanOne(state);
    if (state.index >= state.line.size()) {
      state.pendingNewline = true;
    }
    buffer_.push_back(tok);
    return true;
  }
}

bool Lexer::ensure(size_t lookahead) {
  while (buffer_.size() <= lookahead) {
    if (!refill()) { break; }
  }
  return buffer_.size() > lookahead;
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
void Lexer::buildAll() {
  if (finalized_) { return; }
  finalized_ = true;
  // Process stack in LIFO order
  while (!stack_.empty()) {
    State state = std::move(stack_.back());
    stack_.pop_back();
    state.indentStack = {0}; state.line.clear(); state.index = 0; state.lineNo = 0; state.needIndentCheck = true; state.pendingNewline = false;
    // Read each line
    while (readNextLine(state)) {
      // Handle indent/dedent and skip empty/comment lines
      std::vector<Token> indents;
      if (emitIndentTokens(state, indents, 1)) {
        tokens_.push_back(indents.front());
        continue;
      }
      for (auto& tok : indents) { tokens_.push_back(tok); }

      // Tokenize rest of the line
      while (state.index < state.line.size()) {
        const Token tok = scanOne(state);
        // scanOne may return newline for comments/empty trailing
        if (tok.kind == TokenKind::Newline) { tokens_.push_back(tok); break; }
        tokens_.push_back(tok);
      }
      if (state.index >= state.line.size()) {
        Token newlineTok; newlineTok.kind = TokenKind::Newline; newlineTok.text = "\n"; newlineTok.file = state.src->name(); newlineTok.line = state.lineNo; newlineTok.col = static_cast<int>(state.line.size() + 1);
        tokens_.push_back(newlineTok);
      }
    }
    // flush dedents
    while (state.indentStack.size() > 1) {
      state.indentStack.pop_back();
      Token ded; ded.kind = TokenKind::Dedent; ded.text = "<DEDENT>"; ded.file = state.src->name(); ded.line = state.lineNo + 1; ded.col = 1;
      tokens_.push_back(ded);
    }
  }
  // Final EOF
  Token eof; eof.kind = TokenKind::End; eof.text = "<EOF>"; eof.file = ""; eof.line = 0; eof.col = 1; tokens_.push_back(eof);
}

const Token& Lexer::peek(size_t lookahead) {
  if (!finalized_) { buildAll(); }
  if (pos_ + lookahead < tokens_.size()) {
    return tokens_[pos_ + lookahead];
  }
  return tokens_.back();
}

Token Lexer::next() {
  if (!finalized_) { buildAll(); }
  if (pos_ < tokens_.size()) {
    return tokens_[pos_++];
  }
  return tokens_.back();
}

std::vector<Token> Lexer::tokens() {
  if (!finalized_) { buildAll(); }
  return tokens_;
}

} // namespace pycc::lex
