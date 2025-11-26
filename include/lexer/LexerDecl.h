/**
 * Name: pycc::lex::Lexer
 * Purpose: Stream tokens from a stack of input sources (LIFO).
 */
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "lexer/ITokenStream.h"
#include "lexer/InputSource.h"

namespace pycc::lex {

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

    std::vector<State> stack_{}; // LIFO of inputs
    std::deque<Token> buffer_{}; // lookahead buffer

    // helpers
    bool ensure(size_t lookahead); // ensure buffer has at least k+1 tokens
    bool refill(); // append next token to buffer
    bool readNextLine(State& state); // load next line into state
    bool atEOF() const; // no more inputs
    bool emitIndentTokens(State& state, std::vector<Token>& out, int baseCol);

    Token scanOne(State& state); // scan a single token from current state

    void buildAll(); // build tokens_ from all inputs (LIFO)
};

} // namespace pycc::lex

