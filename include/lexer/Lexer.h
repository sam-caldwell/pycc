/**
 * Name: Lexer headers umbrella
 * Purpose: Provide a stable include that aggregates single-declaration headers.
 * Note: Each header under include/lexer now contains exactly one declaration.
 */
#pragma once

#include "lexer/TokenKind.h"
#include "lexer/Token.h"
#include "lexer/ITokenStream.h"
#include "lexer/InputSource.h"
#include "lexer/FileInput.h"
#include "lexer/StringInput.h"
#include "lexer/LexerDecl.h"
