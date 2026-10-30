#include "./compiler.h"

/* ------------------------------------------------------------------ */
/*  Parser state                                                      */
/* ------------------------------------------------------------------ */

typedef struct {

    Token current;
    Token previous;
    bool hadError;
    bool panicMode;

} Parser;

/* ------------------------------------------------------------------ */
/*  Pratt parser types                                                */
/* ------------------------------------------------------------------ */

typedef enum {
    PREC_NONE,         /* must stay 0: unfilled table entries mean "no rule" */
    PREC_ASSIGNMENT,   /* =        */ // THE MINIMUM PRECEDENCE THRESHOLD
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,     /* == !=    */
    PREC_COMPARISON,   /* < > <= >= */
    PREC_TERM,         /* + -      */
    PREC_FACTOR,       /* * /      */
    PREC_UNARY,        /* - !      */
    PREC_CALL,         /* . ()     */
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(void);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static Parser parser;
static Chunk *compiling_chunk;

/* forward declarations (the rule table and the handlers refer to each other) */
static void expression(void);
static ParseRule *get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static void statement(void);

static Chunk *current_chunk(void)
{
    return compiling_chunk;
}

/*
  2 HELPERS
*/

static char *token_to_string(Token token) {
    char *name = malloc((size_t)token.length + 1);
    if(name == NULL) {
        return NULL;
    }
    memcpy(name, token.start, (size_t)token.length);
    name[token.length] = '\0';
    return name;
}

/* ------------------------------------------------------------------ */
/*  Error reporting                                                   */
/* ------------------------------------------------------------------ */

static void error_at(Token *token, const char *message) {

    if (parser.panicMode)
        return;

    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR) {
        /* nothing */
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error_at_current(const char *message) {
    error_at(&parser.current, message);
}

static void error(const char *message) {
    error_at(&parser.previous, message);
}

/* ------------------------------------------------------------------ */
/*  Token stream                                                      */
/* ------------------------------------------------------------------ */

static void advance(void) {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;
        error_at_current(parser.current.start);
    }
}

static void consume(TokenType type, const char *message) {

    if (parser.current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

/* ------------------------------------------------------------------ */
/*  Bytecode emission                                                 */
/* ------------------------------------------------------------------ */

static void emit_byte(uint8_t byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_return(void) {
    emit_byte(OP_RETURN);
}

static uint8_t make_constant(Value value) {
    int constant = add_constant(current_chunk(), value);

    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emit_constant(Value value) {
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void end_compiler(void) {
    emit_return();
}

/* ------------------------------------------------------------------ */
/*  Parse functions (one per grammar rule)                            */
/* ------------------------------------------------------------------ */

static void number(void) {

    char buffer[64];
    int length = parser.previous.length;

    if (length >= (int)sizeof(buffer)) {
        error("Number literal too long.");
        return;
    }

    memcpy(buffer, parser.previous.start, (size_t)length);
    buffer[length] = '\0';

    if (parser.previous.type == TOKEN_DOUBLE_NUMBER) {
        emit_constant(_8byteF(strtod(buffer, NULL)));
    } else {
        emit_constant(_8byteU(strtoul(buffer, NULL, 10)));
    }
}

static void grouping(void) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(void) {
    TokenType operator_type = parser.previous.type;

    /* compile the operand first */
    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return;
    }
}

/*
  HAVE TO COMPLETE
*/

static void variable(void)
{
    Token name = parser.previous;
    char *var_name = token_to_string(name);

    if(var_name == NULL) {
        error("can not allocate new space for variable name");
    }

    Value value = _string(var_name);
    uint8_t var_index = make_constant(value);

    emit_bytes(OP_GET_VARIBLE, var_index);
}

static void binary(void) {
    TokenType operator_type = parser.previous.type;
    ParseRule *rule = get_rule(operator_type);

    parse_precedence((Precedence)(rule->precedence + 1));

    switch (operator_type) {
        case TOKEN_PLUS:  emit_byte(OP_ADD);      break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:  emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE);   break;
        default: return;
    }
}

/* ------------------------------------------------------------------ */
/*  Rule table: token -> { prefix fn, infix fn, infix precedence }    */
/*  Entries not listed are zero-initialised = no rule.                */
/*  TOKEN_EOF must stay the LAST entry so the table covers every      */
/*  token type that get_rule() can be asked about.                    */
/* ------------------------------------------------------------------ */

static ParseRule rules[] = {
    //                       PREFIX    INFINIX  PRECEDENCE
    [TOKEN_LEFT_PAREN]    = { grouping, NULL,   PREC_NONE   },
    [TOKEN_MINUS]         = { unary,    binary, PREC_TERM   },
    [TOKEN_PLUS]          = { NULL,     binary, PREC_TERM   },
    [TOKEN_SLASH]         = { NULL,     binary, PREC_FACTOR },
    [TOKEN_STAR]          = { NULL,     binary, PREC_FACTOR },
    [TOKEN_NUMBER]        = { number,   NULL,   PREC_NONE   },
    [TOKEN_DOUBLE_NUMBER] = { number,   NULL,   PREC_NONE   },
    [TOKEN_EOF]           = { NULL,     NULL,   PREC_NONE   },
    [TOKEN_IDENTIFIER]    = {variable,  NULL,   PREC_NONE   },
};

static ParseRule *get_rule(TokenType type) {
    return &rules[type];
}

/* ------------------------------------------------------------------ */
/*  The Pratt loop                                                    */
/* ------------------------------------------------------------------ */

static void parse_precedence(Precedence precedence) {
    advance();

    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }
    prefix_rule();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule();
    }
}

static void expression(void) {
    parse_precedence(PREC_ASSIGNMENT);
}

static bool check(TokenType type)
{
    return parser.current.type == type;
}

static bool match(TokenType type) {

    if (!check(type))
        return false;
    advance();
    return true;
}

static TokenType peek(void)
{
    return parser.current.type;
}

static TokenType peek_next(void)
{
    Scanner saved = scanner;      // scanner ka poora state save (start, current, line)
    Token next = scanToken();     // ek token aage scan
    scanner = saved;              // state wapas, jaise kuch hua hi nahi
    return next.type;
}

void allocate_variable_memory() {

    // mem slot/const _8byteU age = 55

    bool FLAG_MUTABLE = false;
    if (match(TOKEN_SLOT))
        FLAG_MUTABLE = true;
    else if (match(TOKEN_CONST))
        FLAG_MUTABLE = false;
    else
        error("Expected 'slot/const' while making variable");

    if (match(TOKEN_8BYTEU)) {

        Token var_name = parser.current;
        consume(TOKEN_IDENTIFIER, "Expect variable name.");

        consume(TOKEN_EQUAL, "Expect '=' after variable name.");
        expression();

        char *name = token_to_string(var_name);
        if (name == NULL) {
            error("Memory allocation failed");
        }

        uint8_t name_constant_index = make_constant(_string(name));

        const char *variable_dtype = "_8byteU";
        uint8_t dtype_index = make_constant(_string(variable_dtype));

        /*
              1 means mutable
              0 means constant
         */

        if (FLAG_MUTABLE) {
            const char *which_type = "mutable";
            uint8_t which_index = make_constant(_string(which_type));
            emit_bytes(OP_MUTABLE_VAR, which_index);
        }

        else {
            const char *which_type = "constant";
            uint8_t which_index = make_constant(_string(which_type));
            emit_bytes(OP_MUTABLE_VAR, which_index);
        }

        emit_bytes(OP_VARIABLE_DTYPE, dtype_index);
        emit_bytes(OP_DEFINE_VARIABLE, name_constant_index);

    } else if (match(TOKEN_8BYTEF)) {

            Token var_name = parser.current;
            consume(TOKEN_IDENTIFIER, "Expect variable name.");

            consume(TOKEN_EQUAL, "Expect '=' after variable name.");
            expression();

            char *name = token_to_string(var_name);
            if (name == NULL) {
                error("Memory allocation failed");
            }

            uint8_t name_constant_index = make_constant(_string(name));

            const char *variable_dtype = "_8byteF";
            uint8_t dtype_index = make_constant(_string(variable_dtype));

            /*
                  1 means mutable
                  0 means constant
            */

            if (FLAG_MUTABLE) {
                const char *which_type = "mutable";
                uint8_t which_index = make_constant(_string(which_type));
                emit_bytes(OP_MUTABLE_VAR, which_index);
            }

            else {
                const char *which_type = "constant";
                uint8_t which_index = make_constant(_string(which_type));
                emit_bytes(OP_MUTABLE_VAR, which_index);
            }

            emit_bytes(OP_VARIABLE_DTYPE, dtype_index);
            emit_bytes(OP_DEFINE_VARIABLE, name_constant_index);

    } else {
        error_at_current("Expected '_8byteU' or '_8byteF'");
        return;
    }
}

bool is_at_end()
{
    return parser.current.type  == TOKEN_EOF;
}

void consume_new_block()
{
    emit_byte(OP_BEGIN_SCOPE);

    while (peek() != TOKEN_RIGHT_BRACE && !is_at_end()) {
        statement();
    }

    if (is_at_end()) {
        error("Expect '}' after block.");
        return;
    }

    advance();
    emit_byte(OP_END_SCOPE);
}

void console_expression()
{
    expression();
    emit_byte(OP_PRINT);
}

void compiler_enable_debug()
{
    emit_byte(OP_DEBUG_ON);
}

void compiler_disable_debug()
{
    emit_byte(OP_DEBUG_OFF);
}

/**********************************************************************/
/*    STATEMENTS                                                      */
/**********************************************************************/

static void statement(void)
{

    if (match(TOKEN_MEMORY)) {
        allocate_variable_memory();

    }

    else if (match(TOKEN_CONSOLE)) {
        console_expression();
    }

    else if (match(TOKEN_DEBUG)) {
        compiler_enable_debug();
    }

    else if (match(TOKEN_OFF_DEBUG)) {
        compiler_disable_debug();
    }

    else if (match(TOKEN_LEFT_BRACE)) {
        consume_new_block();
    }

    else
    {
        expression();
    }
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                       */
/* ------------------------------------------------------------------ */

bool compiler(const char *source, Chunk *chunk) {

    init_scanner(source);
    compiling_chunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (parser.current.type != TOKEN_EOF) {
        statement();
        if (parser.hadError)
            break;
    }

    end_compiler();
    return !parser.hadError;
}
