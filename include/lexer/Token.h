/**
 * Name: pycc::lex::Token
 * Purpose: Token structure with source location and text.
 */
#pragma once

#include <string>
#include "lexer/TokenKind.h"

namespace pycc::lex {

struct Token {
    TokenKind kind{TokenKind::End};
    std::string text{}; // original text (identifier/int/typename)
    std::string file{};
    int line{1}; // 1-based line number
    int col{1}; // 1-based column at token start
};

} // namespace pycc::lex

