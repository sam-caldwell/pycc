/***
 * Name: test_parser_initbuffer_nonlexer
 * Purpose: Cover Parser::initBuffer() fallback path by using a custom ITokenStream.
 */
#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace pycc;

namespace {
class VecStream : public lex::ITokenStream {
 public:
  explicit VecStream(std::vector<lex::Token> toks) : toks_(std::move(toks)) {}
  const lex::Token& peek(size_t k = 0) override {
    size_t idx = pos_ + k; if (idx >= toks_.size()) idx = toks_.size() - 1; return toks_[idx];
  }
  lex::Token next() override {
    if (pos_ < toks_.size()) return toks_[pos_++];
    return toks_.empty() ? lex::Token{} : toks_.back();
  }
 private:
  std::vector<lex::Token> toks_{}; size_t pos_{0};
};
}

TEST(ParserInitBuffer, NonLexerStreamParsesModule) {
  using TK = lex::TokenKind;
  std::vector<lex::Token> v;
  auto t = [&](TK k, const char* txt, int line, int col){ lex::Token x; x.kind=k; x.text=txt; x.file="dummy.py"; x.line=line; x.col=col; return x; };
  // def f() -> int:\n  return 0\n
  v.push_back(t(TK::Def, "def", 1, 1));
  v.push_back(t(TK::Ident, "f", 1, 5));
  v.push_back(t(TK::LParen, "(", 1, 6));
  v.push_back(t(TK::RParen, ")", 1, 7));
  v.push_back(t(TK::Arrow, "->", 1, 9));
  v.push_back(t(TK::TypeIdent, "int", 1, 12));
  v.push_back(t(TK::Colon, ":", 1, 15));
  v.push_back(t(TK::Newline, "\n", 1, 16));
  v.push_back(t(TK::Indent, "<INDENT>", 2, 1));
  v.push_back(t(TK::Return, "return", 2, 3));
  v.push_back(t(TK::Int, "0", 2, 10));
  v.push_back(t(TK::Newline, "\n", 2, 11));
  v.push_back(t(TK::Dedent, "<DEDENT>", 3, 1));
  v.push_back(t(TK::End, "<EOF>", 3, 1));

  VecStream S(v);
  parse::Parser P(S);
  auto mod = P.parseModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_EQ(mod->functions.size(), 1u);
  EXPECT_EQ(mod->functions[0]->name, std::string("f"));
}

