#ifndef FREAX_SCANNER_H
#define FREAX_SCANNER_H

#include "../COMMON/common.h"

typedef enum{
  // Single-character tokens.
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE, TOKEN_COMMA,
  TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS, TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
  TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,

  // One or two character tokens.
  TOKEN_BANG, TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_GREATER,
  TOKEN_GREATER_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,

  // Literals.
  TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_DOUBLE_NUMBER, TOKEN_CHARACTER_ARRAY,

  // Keywords.
  TOKEN_AND, TOKEN_VOID, TOKEN_CONSOLE, TOKEN_MEMORY, TOKEN_8BYTEF, TOKEN_8BYTEU,
  TOKEN_SLOT, TOKEN_CONST, TOKEN_DEBUG, TOKEN_OFF_DEBUG,

  TOKEN_ERROR,
  TOKEN_EOF

} TokenType;

/* Scanner state: no type/length here */
typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

/* NEW: this was missing, hence "unknown type name 'Token'" */
typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

extern Scanner scanner;          /* was: Scanner scanner;  (defined in the header) */

void init_scanner(const char* source);
Token scanToken(void);

#endif
