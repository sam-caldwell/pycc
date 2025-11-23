/**
 * Name: pycc::lex::TokenKind
 * Purpose: Token kinds for the lexer.
 */
#pragma once

namespace pycc::lex {

enum class TokenKind {
    End, // EOF
    Newline, // \n
    Indent, // indentation increase
    Dedent, // indentation decrease

    Def, // def
    Return, // return
    Del, // del
    If, // if
    Else, // else
    Elif, // elif
    While, // while
    For, // for
    In, // in
    Break, // break
    Continue, // continue
    Pass, // pass
    Try, // try
    Except, // except
    Finally, // finally
    With, // with
    As, // as
    Match, // match
    Case, // case
    Import, // import
    From, // from
    Class, // class
    At, // @
    Async, // async
    Assert, // assert
    Raise, // raise
    Global, // global
    Nonlocal, // nonlocal
    Yield, // yield
    Await, // await

    Arrow, // ->
    Colon, // :
    ColonEqual, // := (named expression)
    Comma, // ,
    Equal, // =
    PlusEqual, // +=
    Plus, // +
    MinusEqual, // -=
    Minus, // -
    StarEqual, // *=
    Star, // *
    StarStarEqual, // **=
    StarStar, // ** (power)
    SlashEqual, // /=
    Slash, // /
    SlashSlashEqual, // //=
    SlashSlash, // // (floor-div)
    PercentEqual, // %=
    Percent, // %
    LShiftEqual, // <<=
    LShift, // <<
    RShiftEqual, // >>=
    RShift, // >>
    AmpEqual, // &=
    Amp, // &
    CaretEqual, // ^=
    Caret, // ^
    Tilde, // ~
    PipeEqual, // |=
    EqEq, // ==
    NotEq, // !=
    Lt, // <
    Le, // <=
    Gt, // >
    Ge, // >=
    Is, // is
    And, // and
    Or, // or
    Not, // not
    Lambda, // lambda
    Pipe, // |
    Dot, // .
    LParen, // (
    RParen, // )
    LBracket, // [
    RBracket, // ]
    LBrace, // {
    RBrace, // }

    Ident, // identifier
    Int, // integer literal
    Float, // float literal
    Imag, // imaginary numeric (e.g., 1j)
    String, // string literal
    BoolLit, // True/False

    TypeIdent, // type identifier (int, bool, float, str, None)
    Bytes, // b'...'
    Ellipsis // ...
};

// Convert TokenKind to a stable string for diagnostics/logging
const char* to_string(TokenKind k);

} // namespace pycc::lex

