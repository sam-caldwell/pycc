/**
 * Name: pycc::lex::TokenKind helpers
 * Purpose: Implementation for TokenKind utilities.
 */
#include "lexer/TokenKind.h"

namespace pycc::lex {
    const char *to_string(const TokenKind k) {
        using enum pycc::lex::TokenKind;
        switch (k) {
            case End: return "End";
            case Newline: return "Newline";
            case Indent: return "Indent";
            case Dedent: return "Dedent";
            case Def: return "Def";
            case Return: return "Return";
            case Del: return "Del";
            case If: return "If";
            case Else: return "Else";
            case Elif: return "Elif";
            case While: return "While";
            case For: return "For";
            case In: return "In";
            case Break: return "Break";
            case Continue: return "Continue";
            case Pass: return "Pass";
            case Try: return "Try";
            case Except: return "Except";
            case Finally: return "Finally";
            case With: return "With";
            case As: return "As";
            case Match: return "Match";
            case Case: return "Case";
            case Import: return "Import";
            case From: return "From";
            case Class: return "Class";
            case At: return "At";
            case Async: return "Async";
            case Assert: return "Assert";
            case Raise: return "Raise";
            case Global: return "Global";
            case Nonlocal: return "Nonlocal";
            case Yield: return "Yield";
            case Await: return "Await";
            case Arrow: return "Arrow";
            case Colon: return "Colon";
            case ColonEqual: return "ColonEqual";
            case Comma: return "Comma";
            case Equal: return "Equal";
            case PlusEqual: return "PlusEqual";
            case Plus: return "Plus";
            case MinusEqual: return "MinusEqual";
            case Minus: return "Minus";
            case StarEqual: return "StarEqual";
            case Star: return "Star";
            case StarStarEqual: return "StarStarEqual";
            case StarStar: return "StarStar";
            case SlashEqual: return "SlashEqual";
            case Slash: return "Slash";
            case SlashSlashEqual: return "SlashSlashEqual";
            case SlashSlash: return "SlashSlash";
            case PercentEqual: return "PercentEqual";
            case Percent: return "Percent";
            case LShiftEqual: return "LShiftEqual";
            case LShift: return "LShift";
            case RShiftEqual: return "RShiftEqual";
            case RShift: return "RShift";
            case AmpEqual: return "AmpEqual";
            case Amp: return "Amp";
            case CaretEqual: return "CaretEqual";
            case Caret: return "Caret";
            case Tilde: return "Tilde";
            case PipeEqual: return "PipeEqual";
            case EqEq: return "EqEq";
            case NotEq: return "NotEq";
            case Lt: return "Lt";
            case Le: return "Le";
            case Gt: return "Gt";
            case Ge: return "Ge";
            case Is: return "Is";
            case And: return "And";
            case Or: return "Or";
            case Not: return "Not";
            case Lambda: return "Lambda";
            case LParen: return "LParen";
            case RParen: return "RParen";
            case LBracket: return "LBracket";
            case RBracket: return "RBracket";
            case LBrace: return "LBrace";
            case RBrace: return "RBrace";
            case Pipe: return "Pipe";
            case Dot: return "Dot";
            case Ident: return "Ident";
            case Int: return "Int";
            case Float: return "Float";
            case Imag: return "Imag";
            case String: return "String";
            case BoolLit: return "BoolLit";
            case TypeIdent: return "TypeIdent";
            case Bytes: return "Bytes";
            case Ellipsis: return "Ellipsis";
        }
        return "Unknown";
    }
} // namespace pycc::lex
