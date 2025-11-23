/**
 * Name: pycc::lex::TokenKind helpers
 * Purpose: Implementation for TokenKind utilities.
 */
#include "lexer/TokenKind.h"

namespace pycc::lex {

const char* to_string(const TokenKind k) {
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
        case TokenKind::Async: return "Async";
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
        case TokenKind::Imag: return "Imag";
        case TokenKind::String: return "String";
        case TokenKind::BoolLit: return "BoolLit";
        case TokenKind::TypeIdent: return "TypeIdent";
        case TokenKind::Bytes: return "Bytes";
        case TokenKind::Ellipsis: return "Ellipsis";
    }
    return "Unknown";
}

} // namespace pycc::lex

