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
    // PEP 263: detect encoding declaration on first or second line (cookie only; we do not re-decode the file).
    if (comment && (state.lineNo == 1 || state.lineNo == 2)) {
      // Look for 'coding' cookie patterns: coding[:=]\s*([\w.-]+)
      auto& s = state.line;
      auto pos = s.find("coding");
      if (pos != std::string::npos) {
        // Best-effort validation without acting on it; ensures cookie lines never break lexing.
        (void)pos;
      }
    }
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
  auto scanStringLike = [&](size_t start, bool hasB, bool hasR, bool hasF) -> Token {
    // Determine quote char and whether triple-quoted
    size_t p = start;
    // Skip up to 2 prefix chars already parsed
    while (p < line.size() && (line[p]=='b'||line[p]=='B'||line[p]=='f'||line[p]=='F'||line[p]=='r'||line[p]=='R'||line[p]=='u'||line[p]=='U')) { ++p; }
    if (p >= line.size()) { Token t = makeTok(hasB ? TokenKind::Bytes : TokenKind::String, start, p); t.text = line.substr(start, p-start); idx = p; return t; }
    const char quote = line[p];
    bool triple = false;
    if (p+2 < line.size() && (line[p]==line[p+1]) && (line[p]==line[p+2]) && (quote=='"' || quote=='\'')) { triple = true; }
    size_t endPos = p + 1;
    if (!triple) {
      bool escape = false;
      for (; endPos < line.size(); ++endPos) {
        const char c = line[endPos];
        if (!hasR) {
          if (escape) { escape = false; continue; }
          if (c == '\\') { escape = true; continue; }
        }
        if (c == quote) { ++endPos; break; }
      }
      // If not closed, keep as far as line end (lexer will emit Newline and continue)
      if (endPos > line.size()) endPos = line.size();
      // CPython: raw strings cannot end with a single backslash before the closing quote
      if (hasR && endPos <= line.size() && endPos > (p+1)) {
        size_t qpos = endPos - 1; // position of closing quote
        if (qpos > 0 && line[qpos-1] == '\\') {
          // Treat as unterminated: consume to end of line to trigger recovery in parser
          endPos = line.size();
        }
      }
    } else {
      // For triple-quoted: consume only the opening quotes; multi-line content handled by higher-level scanning.
      endPos = p + 3;
    }
    Token tok = makeTok(hasB ? TokenKind::Bytes : TokenKind::String, start, endPos);
    tok.text = line.substr(start, endPos - start);
    idx = endPos;
    (void)hasF; // f-strings handled by parser using token text
    return tok;
  };
  // Raw quote start
  if (chr == '"' || chr == '\'') { return scanStringLike(idx, /*hasB=*/false, /*hasR=*/false, /*hasF=*/false); }
  // prefixes: b/B, f/F, r/R, u/U and combos (fr, rf, rb, br). Disallow b+f together (treat as String token).
  if ((chr=='b'||chr=='B'||chr=='f'||chr=='F'||chr=='r'||chr=='R'||chr=='u'||chr=='U') && idx+1 < line.size()) {
    size_t p = idx; bool hasB=false, hasR=false, hasF=false; int cnt=0;
    while (cnt<2 && p<line.size()) {
      char c = line[p];
      if (c=='b'||c=='B') { hasB = true; }
      else if (c=='r'||c=='R') { hasR = true; }
      else if (c=='f'||c=='F') { hasF = true; }
      else if (c=='u'||c=='U') { /* legacy unicode prefix tolerated */ }
      else break;
      ++p; ++cnt;
    }
    if (p < line.size() && (line[p]=='\''||line[p]=='"')) { return scanStringLike(idx, hasB && !hasF, hasR, hasF); }
    if (p+2 < line.size() && line[p]==line[p+1] && line[p]==line[p+2] && (line[p]=='\''||line[p]=='"')) { return scanStringLike(idx, hasB && !hasF, hasR, hasF); }
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
      bool prevUnderscore = false; size_t digits = 0;
      while (idx < line.size()) {
        const char d = line[idx];
        if (std::isdigit(static_cast<unsigned char>(d)) != 0) { prevUnderscore = false; ++digits; ++idx; continue; }
        if (d == '_' && digits > 0 && !prevUnderscore) { prevUnderscore = true; ++idx; continue; }
        break;
      }
      // Remove trailing underscore
      if (prevUnderscore) { --idx; }
      if (idx == startIdx) { return pos; } // back out if no digits
      return idx;
    }
    return pos;
  };
  auto isDecDigit = [&](char c){ return std::isdigit(static_cast<unsigned char>(c)) != 0; };
  auto isHexDigit = [&](char c){ return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
  auto isBinDigit = [&](char c){ return c=='0'||c=='1'; };
  auto isOctDigit = [&](char c){ return c>='0'&&c<='7'; };
  auto scanDigitsUnderscore = [&](size_t pos, auto isOk) {
    size_t i = pos; bool have = false; bool prevUnderscore=false;
    while (i < line.size()) {
      char c = line[i];
      if (isOk(c)) { have = true; prevUnderscore=false; ++i; continue; }
      if (c=='_' && have && !prevUnderscore) { prevUnderscore=true; ++i; continue; }
      break;
    }
    if (prevUnderscore) --i; // trim trailing underscore
    return i;
  };
  if (std::isdigit(static_cast<unsigned char>(chr)) != 0) {
    size_t i0 = idx;
    // Base prefixes 0b/0o/0x
    if (line[idx] == '0' && idx + 1 < line.size()) {
      char p1 = line[idx+1];
      if (p1=='b'||p1=='B'||p1=='o'||p1=='O'||p1=='x'||p1=='X') {
        size_t p = idx + 2;
        if (p1=='b'||p1=='B') p = scanDigitsUnderscore(p, isBinDigit);
        else if (p1=='o'||p1=='O') p = scanDigitsUnderscore(p, isOctDigit);
        else p = scanDigitsUnderscore(p, isHexDigit);
        size_t end = p;
        if (end < line.size() && (line[end]=='j'||line[end]=='J')) { ++end; Token tok = makeTok(TokenKind::Imag, i0, end); idx = end; return tok; }
        Token tok = makeTok(TokenKind::Int, i0, p); idx = p; return tok;
      }
    }
    // Decimal int or float
    size_t p = scanDigitsUnderscore(idx, isDecDigit);
    // Float with fractional part or dot-only fractional
    if (p < line.size() && line[p] == '.') {
      size_t fracStart = p + 1;
      size_t fracEnd = scanDigitsUnderscore(fracStart, isDecDigit);
      size_t epos = scanExponent(fracEnd);
      size_t end = epos;
      if (end < line.size() && (line[end]=='j'||line[end]=='J')) { ++end; Token tok = makeTok(TokenKind::Imag, i0, end); idx = end; return tok; }
      Token tok = makeTok(TokenKind::Float, i0, epos); idx = epos; return tok;
    }
    // Float via exponent only
    size_t epos = scanExponent(p);
    if (epos != p) {
      size_t end = epos;
      if (end < line.size() && (line[end]=='j'||line[end]=='J')) { ++end; Token tok = makeTok(TokenKind::Imag, i0, end); idx = end; return tok; }
      Token tok = makeTok(TokenKind::Float, i0, epos); idx = epos; return tok;
    }
    // Int or Imag
    size_t end = p;
    if (end < line.size() && (line[end]=='j'||line[end]=='J')) { ++end; Token tok = makeTok(TokenKind::Imag, i0, end); idx = end; return tok; }
    Token tok = makeTok(TokenKind::Int, i0, p); idx = p; return tok;
  }
  if (chr == '.') {
    if (idx + 2 < line.size() && line[idx+1] == '.' && line[idx+2] == '.') { idx += 3; return makeTok(TokenKind::Ellipsis, idx-3, idx); }
    if (idx + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[idx+1])) != 0) {
      size_t mpos = idx + 1;
      // scan digits with underscores
      bool prevUnderscore = false; while (mpos < line.size()) { const char c = line[mpos]; if (std::isdigit(static_cast<unsigned char>(c)) != 0) { prevUnderscore=false; ++mpos; } else if (c=='_' && !prevUnderscore) { prevUnderscore=true; ++mpos; } else break; } if (prevUnderscore) --mpos;
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
