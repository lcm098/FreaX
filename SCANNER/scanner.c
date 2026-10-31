#include "./scanner.h"

Scanner scanner;

void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}


static bool is_at_end(){
    return*scanner.current=='\0';
}

static Token make_token(TokenType type) {

    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;

    return token;
}

static Token error_token(const char* message){
    Token token;
    token.type= TOKEN_ERROR;
    token.start= message;
    token.length=(int)strlen(message);
    token.line= scanner.line;

    return token;
}

static char advance(){
   scanner.current++;
   return scanner.current[-1];
}

static bool match(char expected){
  if(is_at_end()) return false;
  if(*scanner.current!= expected) return false;

  scanner.current++;
  return true;
}

static char peek(){
   return*scanner.current;
}

static char peek_next(){
    if(is_at_end())
        return'\0';

    return scanner.current[1];
}

static void skip_white_space(){
  for(;;){
    char c= peek();
    switch(c){

    case' ':
    case'\r':
    case'\t':
        advance();
        break;

    case'\n':
        scanner.line++;
        advance();
        break;

    case '/':

        if(peek_next() == '/') {
            while(peek() != '\n' && !is_at_end()) {
                advance();
            }
        }

        else if (peek_next() == '*') {
            advance();
            advance();

            while (!(peek() == '*' && peek_next() == '/') && !is_at_end()) {
                advance();
            }

            if (is_at_end()) {
                fprintf(stderr, "Unterminated multiline comment\n");
                exit(EXIT_FAILURE);
            }

            advance();
            advance();
        }

        else {
            return;
        }
        break;

      default:
        return;
    }
  }
}

static Token string() {
    while(peek() != '"' && !is_at_end()) {
        if(peek() == '\n')
            scanner.line++;

        advance();
    }
    if(is_at_end())
        return error_token("unterminated string");
    advance();
    return make_token(TOKEN_STRING);
}

static bool is_digit(char c){
    return c>='0'&& c<='9';
}

static Token number(){

    bool FLAG_DOUBLE = false;

    while(is_digit(peek()))
        advance();

    if(peek()=='.'&& is_digit(peek_next())){
        advance();
        FLAG_DOUBLE = true;
        while(is_digit(peek()))
            advance();
    }

    if (FLAG_DOUBLE) {
        return make_token(TOKEN_DOUBLE_NUMBER);
    }
    return make_token(TOKEN_NUMBER);
}

static bool is_alpha(char c){
    return(c>='a'&& c<='z') || (c>='A'&& c<='Z') || c=='_';
}

static TokenType check_keyword(int start, int length, const char* rest, TokenType type){
    if(scanner.current - scanner.start == start+length &&
       memcmp(scanner.start+start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(){

    switch(scanner.start[0]) {

    case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
    case 'v': return check_keyword(1, 3, "oid", TOKEN_VOID);
    case 'c': {
        TokenType tok = check_keyword(1, 4, "onst", TOKEN_CONST);
        if (tok != TOKEN_IDENTIFIER) {
            return tok;
        }
        return check_keyword(1, 6, "onsole", TOKEN_CONSOLE);
    }
    case 'm': return check_keyword(1, 2, "em", TOKEN_MEMORY);
    case 'd': return check_keyword(1, 4, "ebug", TOKEN_DEBUG);
    case 'J': return check_keyword(1, 3, "UMP", TOKEN_JUMP);
    case '_': {

        TokenType tok;

        tok = check_keyword(1, 6, "string", TOKEN_STRING_);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "1byteI", TOKEN_1BYTEI);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "1byteU", TOKEN_1BYTEU);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "2byteI", TOKEN_2BYTEI);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "2byteU", TOKEN_2BYTEU);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "4byteI", TOKEN_4BYTEI);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "4byteU", TOKEN_4BYTEU);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "8byteI", TOKEN_8BYTEI);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "8byteU", TOKEN_8BYTEU);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 7, "16byteI", TOKEN_16BYTEI);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 7, "16byteU", TOKEN_16BYTEU);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "4byteF", TOKEN_4BYTEF);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 6, "8byteF", TOKEN_8BYTEF);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        tok = check_keyword(1, 7, "16byteF", TOKEN_16BYTEF);
        if (tok != TOKEN_IDENTIFIER)
            return tok;

        return check_keyword(1, 6, "1byteC", TOKEN_1BYTEC);
    }
    case 'o': return check_keyword(1, 8, "ff_debug", TOKEN_OFF_DEBUG);
    case 's': return check_keyword(1, 3, "lot", TOKEN_SLOT);
    case 'i': return check_keyword(1, 1, "f",   TOKEN_IF);
    case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);

    }

    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while(is_alpha(peek()) || is_digit(peek())) {
        advance();
    }
    return make_token(identifier_type());
}

Token scanToken() {

    skip_white_space();

    scanner.start = scanner.current;
    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    char c = advance();

    if (is_alpha(c))
        return identifier();

    if (is_digit(c))
        return number();

    switch(c) {
    case '(': return make_token(TOKEN_LEFT_PAREN);
    case ')': return make_token(TOKEN_RIGHT_PAREN);
    case '{': return make_token(TOKEN_LEFT_BRACE);
    case '}': return make_token(TOKEN_RIGHT_BRACE);
    case ';': return make_token(TOKEN_SEMICOLON);
    case ',': return make_token(TOKEN_COMMA);
    case '.': return make_token(TOKEN_DOT);
    case '/': return make_token(TOKEN_SLASH);
    case '*': return make_token(TOKEN_STAR);
    case '[': return make_token(TOKEN_LEFT_BRACKET);
    case ']': return make_token(TOKEN_RIGHT_BRACKET);
    case ':': return make_token(TOKEN_COLON);

    case '!':
        return make_token(match('=')? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return make_token(match('=')? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
        return make_token(match('=')? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
        return make_token(match('=')? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '-':
        return make_token(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
    case '+':
        return make_token(match('=') ? TOKEN_PLUS_EQUAL  : TOKEN_PLUS);

    case '&':
        return match('&') ? make_token(TOKEN_AND_AND)
                          : error_token("Unexpected character '&' (did you mean '&&'?)");
    case '|':
        return match('|') ? make_token(TOKEN_OR_OR)
                          : error_token("Unexpected character '|' (did you mean '||'?)");

    case '"':
        return string();

    }

    return error_token("Unexpected character");
}
