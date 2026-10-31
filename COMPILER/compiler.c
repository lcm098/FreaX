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

/* one-token lookahead: token AFTER parser.current (not used yet) */
__attribute__((unused))
static TokenType peek_next(void)
{
    Scanner saved = scanner;      /* save scanner state (start, current, line) */
    Token next = scanToken();     /* scan one token ahead                      */
    scanner = saved;              /* restore, as if nothing happened           */
    return next.type;
}

static bool is_at_end(void)
{
    return parser.current.type == TOKEN_EOF;
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

/* heap copy of text -> string constant (all strings are malloc'ed: uniform ownership) */
static uint8_t string_constant(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        error("Memory allocation failed.");
        return 0;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return make_constant(_string(copy));
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

/* ---------- number literal ---------- */
/*
   A literal carries no type:
     integer  -> _8byteI  (or _16byteI / _16byteU when it does not fit)
     decimal  -> _8byteF  (or _16byteF when it has more than 15 digits)
   The declared type (_1byteU etc.) is applied by the VM at OP_DEFINE_VARIABLE.
*/

/* decimal digits -> unsigned 128-bit. false = overflow (> UINT128_MAX) */

static bool parse_u128(const char *s, __uint128_t *out)
{
    const __uint128_t MAX = ~(__uint128_t)0;
    __uint128_t n = 0;

    for (; *s; s++) {
        unsigned digit = (unsigned)(*s - '0');
        if (n > (MAX - digit) / 10)
            return false;
        n = n * 10 + digit;
    }

    *out = n;
    return true;
}

static void number(void) {
    char buffer[128];
    int length = parser.previous.length;

    if (length >= (int)sizeof(buffer)) {
        error("Number literal too long.");
        return;
    }

    memcpy(buffer, parser.previous.start, (size_t)length);
    buffer[length] = '\0';

    switch (parser.previous.type) {

        case TOKEN_NUMBER: {
            __uint128_t n;
            if (!parse_u128(buffer, &n)) {
                error("Integer literal too large for 16 bytes.");
                return;
            }

            const __uint128_t INT64_LIMIT  = (__uint128_t)INT64_MAX;
            const __uint128_t INT128_LIMIT = (((__uint128_t)1) << 127) - 1;

            if (n <= INT64_LIMIT)
                emit_constant(_8byteI((int64_t)n));
            else if (n <= INT128_LIMIT)
                emit_constant(_16byteI((__int128)n));
            else
                emit_constant(_16byteU(n));
            break;
        }

        case TOKEN_DOUBLE_NUMBER: {
            /* double remembers ~15-16 digits, __float128 ~33 */
            int digits = 0;
            for (const char *p = buffer; *p; p++)
                if (*p >= '0' && *p <= '9')
                    digits++;

            if (digits > 15)
                emit_constant(_16byteF(strtoflt128(buffer, NULL)));
            else
                emit_constant(_8byteF(strtod(buffer, NULL)));
            break;
        }

        default:
            error("Invalid numeric literal.");
            break;
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

/* ---------- variable read ---------- */

static void variable(void)
{
    uint8_t name_index = string_constant(parser.previous.start,
                                         (size_t)parser.previous.length);
    emit_bytes(OP_GET_VARIBLE, name_index);
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
    //                       PREFIX    INFIX    PRECEDENCE
    [TOKEN_LEFT_PAREN]    = { grouping, NULL,   PREC_NONE   },
    [TOKEN_MINUS]         = { unary,    binary, PREC_TERM   },
    [TOKEN_PLUS]          = { NULL,     binary, PREC_TERM   },
    [TOKEN_SLASH]         = { NULL,     binary, PREC_FACTOR },
    [TOKEN_STAR]          = { NULL,     binary, PREC_FACTOR },
    [TOKEN_NUMBER]        = { number,   NULL,   PREC_NONE   },
    [TOKEN_DOUBLE_NUMBER] = { number,   NULL,   PREC_NONE   },
    [TOKEN_IDENTIFIER]    = { variable, NULL,   PREC_NONE   },
    [TOKEN_EOF]           = { NULL,     NULL,   PREC_NONE   },
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

/* ------------------------------------------------------------------ */
/*  Declarations:  mem slot|const <datatype> name = expr              */
/* ------------------------------------------------------------------ */

static bool is_datatype_token(TokenType type) {
    return type >= TOKEN_1BYTEI && type <= TOKEN_1BYTEC;
}

static void allocate_variable_memory(void)
{
    bool is_mutable;

    if (match(TOKEN_SLOT)) {
        is_mutable = true;
    } else if (match(TOKEN_CONST)) {
        is_mutable = false;
    } else {
        error_at_current("Expected 'slot' or 'const' after 'mem'.");
        return;
    }

    if (!is_datatype_token(parser.current.type)) {
        error_at_current("Expected a datatype (e.g. _8byteU) after 'slot/const'.");
        return;
    }

    advance();

    /* "_1byteU", "_8byteF", ... */
    Token dtype_token = parser.previous;

    Token var_name = parser.current;
    consume(TOKEN_IDENTIFIER, "Expect variable name.");
    consume(TOKEN_EQUAL, "Expect '=' after variable name.");

    // emit value
    expression();

    if (parser.hadError)
        return;

    const char *mutability = is_mutable ? "mutable" : "constant";

    uint8_t name_index  = string_constant(var_name.start,    (size_t)var_name.length);
    uint8_t dtype_index = string_constant(dtype_token.start, (size_t)dtype_token.length);
    uint8_t mut_index   = string_constant(mutability,        strlen(mutability));

    /* VM stack order:  value, mutability, dtype   (dtype on top) */
    emit_bytes(OP_MUTABLE_VAR,     mut_index);
    emit_bytes(OP_VARIABLE_DTYPE,  dtype_index);
    emit_bytes(OP_DEFINE_VARIABLE, name_index);
}

/* ------------------------------------------------------------------ */
/*  Blocks, console, debug                                            */
/* ------------------------------------------------------------------ */

static void consume_new_block(void)
{
    emit_byte(OP_BEGIN_SCOPE);

    while (peek() != TOKEN_RIGHT_BRACE && !is_at_end() && !parser.hadError) {
        statement();
    }

    if (parser.hadError)
        return;

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    emit_byte(OP_END_SCOPE);
}

static void console_expression(void)
{
    expression();
    emit_byte(OP_PRINT);
}

static void compiler_enable_debug(void)
{
    emit_byte(OP_DEBUG_ON);
}

static void compiler_disable_debug(void)
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

    else {
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
