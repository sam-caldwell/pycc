/**
 * Name: pycc::lex::ITokenStream
 * Purpose: Abstract interface for token streams.
 */
#pragma once

#include <cstddef>
#include "lexer/Token.h"

namespace pycc::lex {

class ITokenStream {
public:
    virtual ~ITokenStream() = default;

    virtual const Token& peek(size_t k = 0) = 0; // lookahead k (0=current)
    virtual Token next() = 0; // consume next token
};

} // namespace pycc::lex

