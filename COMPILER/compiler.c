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

/*

    COMPILE TIME LABEL JUMP EVALUATOR
*/
typedef struct
{
    char *name;
    uint32_t address;
} CompilerLabel;

typedef struct
{
    char *name;
    size_t operand_offset;
    int line;
} PendingJump;


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
static Chunk *current_chunk(void);
static void error(const char *message);

static CompilerLabel *labels = NULL;
static size_t label_count = 0;
static size_t label_capacity = 0;

static PendingJump *pending_jumps = NULL;
static size_t pending_jump_count = 0;
static size_t pending_jump_capacity = 0;
static Chunk *current_chunk(void);


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

/* ---------- jumps for if/else, && and || ----------
   Same format as OP_JUMP: opcode + 2-byte ABSOLUTE address (high byte first).
   emit_jump() writes a placeholder and returns where the operand lives;
   patch_jump() later fills it with "the address of the next byte to be emitted". */

static int emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xFF);
    emit_byte(0xFF);
    return current_chunk()->count - 2;
}

static void patch_jump(int offset) {
    int target = current_chunk()->count;

    if (target > UINT16_MAX) {
        error("Too much code to jump over.");
        return;
    }

    current_chunk()->code[offset]     = (uint8_t)((target >> 8) & 0xFF);
    current_chunk()->code[offset + 1] = (uint8_t)(target & 0xFF);
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
        case TOKEN_BANG:  emit_byte(OP_NOT);    break;
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

        case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL);         break;
        case TOKEN_BANG_EQUAL:    emit_byte(OP_NOT_EQUAL);     break;
        case TOKEN_GREATER:       emit_byte(OP_GREATER);       break;
        case TOKEN_GREATER_EQUAL: emit_byte(OP_GREATER_EQUAL); break;
        case TOKEN_LESS:          emit_byte(OP_LESS);          break;
        case TOKEN_LESS_EQUAL:    emit_byte(OP_LESS_EQUAL);    break;
        default: return;
    }
}

/* a && b : if a is falsey we skip b (short-circuit); the result is always a bool */
static void and_(void) {
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);   /* a stays on the stack */
    emit_byte(OP_POP);                            /* a was truthy: drop it, evaluate b */

    parse_precedence((Precedence)(PREC_AND + 1));

    patch_jump(end_jump);
    emit_byte(OP_TO_BOOL);
}

/* a || b : if a is truthy we skip b (short-circuit); the result is always a bool */
static void or_(void) {
    int end_jump = emit_jump(OP_JUMP_IF_TRUE);
    emit_byte(OP_POP);

    parse_precedence((Precedence)(PREC_OR + 1));

    patch_jump(end_jump);
    emit_byte(OP_TO_BOOL);
}

static void string_seq()
{
    int length = parser.previous.length - 2;
    char *value = malloc(length + 1);

    memcpy(value, parser.previous.start + 1, length);
    value[length] = '\0';
    emit_constant(_string(value));
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
    [TOKEN_STRING]        = { string_seq, NULL,   PREC_NONE   },

    [TOKEN_BANG]          = { unary,    NULL,   PREC_NONE       },
    [TOKEN_BANG_EQUAL]    = { NULL,     binary, PREC_EQUALITY   },
    [TOKEN_EQUAL_EQUAL]   = { NULL,     binary, PREC_EQUALITY   },
    [TOKEN_GREATER]       = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_LESS]          = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]    = { NULL,     binary, PREC_COMPARISON },
    [TOKEN_AND_AND]       = { NULL,     and_,   PREC_AND        },
    [TOKEN_OR_OR]         = { NULL,     or_,    PREC_OR         },
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
    return type >= TOKEN_1BYTEI && type <= TOKEN_STRING_;
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

/**********************************************************************************/
/*          BACK PACHING           */

static char *copy_name(const char *text, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static void reset_label_tables(void)
{
    for (size_t i = 0; i < label_count; i++)        free(labels[i].name);
    for (size_t i = 0; i < pending_jump_count; i++) free(pending_jumps[i].name);
    free(labels);        labels = NULL;        label_count = label_capacity = 0;
    free(pending_jumps); pending_jumps = NULL; pending_jump_count = pending_jump_capacity = 0;
}

static CompilerLabel *find_label(const char *name)
{
    for (size_t i = 0; i < label_count; i++)
        if (strcmp(labels[i].name, name) == 0)
            return &labels[i];
    return NULL;
}

static void define_label(Token id)
{
    /*
        Copy the label name into separately allocated memory.
        The token points to the source code, but the label table
        needs to maintain its own copy.
    */
    char *name = copy_name(id.start, (size_t)id.length);
    if (name == NULL) {
        error("Memory allocation failed.");
        return;
    }

    /*
        Labels must have unique names.
        If the name already exists, reject this definition.
    */
    if (find_label(name)) {
        error("Duplicate label.");
        free(name);
        return;
    }

    /*
        Ensure the label table has an available slot.
        If full, double its capacity (initial capacity is 8).
    */
    if (label_count == label_capacity) {

        label_capacity = label_capacity ? label_capacity * 2 : 8;
        labels = realloc(labels, label_capacity * sizeof(CompilerLabel));

        if (labels == NULL) {
            error("Memory allocation failed.");
            return;
        }
    }

    /*
        Register the label:
        - name    = label identifier.
        - address = current bytecode count, i.e. the index where
                    the next instruction will be emitted.

        Labels emit no bytecode; they only mark an address.
    */
    labels[label_count].name    = name;
    labels[label_count].address = (uint32_t)current_chunk()->count;

    // Increase the number of registered labels.
    label_count++;
}

static void add_pending_jump(Token id, size_t operand_offset)
{
    char *name = copy_name(id.start, (size_t)id.length);

    if (name == NULL)
    {
        error("Memory allocation failed.");
        return;
    }

    if (pending_jump_count == pending_jump_capacity) {

        pending_jump_capacity = pending_jump_capacity ? pending_jump_capacity * 2 : 8;
        pending_jumps = realloc(pending_jumps, pending_jump_capacity * sizeof(PendingJump));

        if (pending_jumps == NULL)
        {
            error("Memory allocation failed.");
            return;
        }
    }

    pending_jumps[pending_jump_count].name           = name;
    pending_jumps[pending_jump_count].operand_offset = operand_offset;
    pending_jumps[pending_jump_count].line           = id.line;
    pending_jump_count++;
}

/* the backpatching pass: runs once, after all code is emitted */
/* this function invoked by compiler at last of compiling to ensure we captured
   every labels and jump


   REFERENCE:

   typedef struct
    {
        char *name;
        uint32_t address;
    } CompilerLabel;

    typedef struct
    {
        char *name;
        size_t operand_offset;
        int line;
    } PendingJump;


    static CompilerLabel *labels = NULL;

    static size_t label_count = 0;
    static size_t label_capacity = 0;

    static PendingJump *pending_jumps = NULL;

    static size_t pending_jump_count = 0;
    static size_t pending_jump_capacity = 0;
    static Chunk *current_chunk(void);
*/

static void resolve_jumps(void)
{
    for (size_t i = 0; i < pending_jump_count; i++) {

        PendingJump *j = &pending_jumps[i];

        /*
              find_label takes string as name return CompilingLabel struct-Object
              of correct (counting 1) label information. and return NULL if fail.

              PendingJump *j - points on:
                                           j->name
                                           j->operand_offset
                                           j->line

             basically final_label find corresponding label against name.
        */

        CompilerLabel *label = find_label(j->name);

        if (label == NULL) {
            fprintf(stderr, "[line %d] Error: undefined label '%s'.\n", j->line, j->name);
            parser.hadError = true;
            continue;
        }

        if (label->address > UINT16_MAX) {
            fprintf(stderr,
                    "[line %d] Error: label '%s' is out of jump range.\n",
                    j->line, j->name);
            parser.hadError = true;
            continue;
        }

        /*
              current_chunk()->code - means get the *FIRST* address of bytecode array
              uint8_t *code - points the first byte.

              *IMPORTANT*
              label->address - holds the actual address in bytecode array where we want to
                               jump.

            The problem is address store in uint32_t (4bytes) but we store the
            jump address in 2 bytes (uint16_t).

            so earlier we stored 0xFF (uint16_t) + 0xFF (uint16_t) 0xFF = uint32_t;

            High byte — upper 8 bits.
            Low byte — lower 8 bits.

            *ASSUME:*

                We have a label named "START" whose bytecode address is 13.

                    labels[label_count].name    = "START";
                    labels[label_count].address = 13;

                Decimal 13 in binary (16 bits):

                    00000000 00001101
                    |------| |------|
                    High     Low
                    byte     byte

                HIGH BYTE:

                    (label->address >> 8) & 0xFF

                    First, >> 8 shifts all bits 8 positions to the right:

                        00000000 00001101
                        >> 8
                        -----------------
                        00000000 00000000

                    Then, & 0xFF keeps only the lower 8 bits.

                    Result = 0

                LOW BYTE:

                    label->address & 0xFF

                    00000000 00001101
                    00000000 11111111  (0xFF)
                    -----------------
                    00000000 00001101

                    Result = 13

                *FINAL BACKPATCHING:*

                    code[j->operand_offset]     = 0;  // High byte
                    code[j->operand_offset + 1] = 13; // Low byte

                Now the two placeholder bytes have been replaced with the actual
                jump address: 13.

                IMPORTANT:
                0xFF is a single byte (8 bits), not uint16_t.
                Two 0xFF bytes together reserve 16 bits (2 bytes).

        */

        uint8_t *code = current_chunk()->code;
        code[j->operand_offset]     = (label->address >> 8) & 0xFF;
        code[j->operand_offset + 1] =  label->address       & 0xFF;
    }
}

/*
compiling_chunk =

typedef struct {
    uint8_t *code;
    int count;
    int capacity;
    int *lines;
    ValueArray constants;
}Chunk;

Chunk means Bytecode Array
and emit_byte and emit_bytes invoke write_chunk function
and write_chunk function write in Chunk Array (bytecode array)

why current_chunk()->count - 2 ?
because of:

emit_byte(OP_JUMP);  // giving this addess
emit_byte(0xFF);     - 1
emit_byte(0xFF);     - 1

ASSUME in bytecode array:
if INDEX 10 = OP_JUMP
INDEX 11 = Address HIGH byte
INDEX 12 = ADDRESS LOW byte

we need 11! but count is 13, so 13-2 = 11

Quick!
write_chunk writes the bytecode and increment the count by one and line by 1

*/

static Chunk *current_chunk(void)
{
    return compiling_chunk;
}


void consume_label(void)
{
    Token id = parser.current;
    if (!match(TOKEN_IDENTIFIER)) {
        error_at_current("Expected identifier after ':' as label.");
        return;
    }

    /* compile-time only: emits NO bytecode */
    define_label(id);
}

void consume_jump(void)
{
    Token id = parser.current;

    if (!match(TOKEN_IDENTIFIER)) {
        error_at_current("Expected label name after JUMP.");
        return;
    }
    emit_byte(OP_JUMP);
    emit_byte(0xFF);           /* placeholder: high byte */
    emit_byte(0xFF);           /* placeholder: low byte  */
    add_pending_jump(id, current_chunk()->count - 2);
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

/*
    if (condition) statement [else statement]

        <condition>
        JUMP_IF_FALSE  --------------+
        POP                          |
        <then>                       |
        JUMP  -------------------+   |
                                 |   |
        POP   <------------------|---+     (else starts here)
        <else>                   |
        ...   <------------------+

    "else if" needs no special code: it is just  else + another if-statement.
*/
static void if_statement(void)
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);                    /* condition true: drop it */
    statement();

    int else_jump = emit_jump(OP_JUMP);   /* skip the else part */

    patch_jump(then_jump);
    emit_byte(OP_POP);                    /* condition false: drop it */

    if (match(TOKEN_ELSE))
        statement();

    patch_jump(else_jump);
}

static bool is_assign_op(TokenType t) {
    return t == TOKEN_EQUAL || t == TOKEN_PLUS_EQUAL || t == TOKEN_MINUS_EQUAL;
}

/*  name = expr      ->  <expr>                 SET name
    name += expr     ->  GET name  <expr>  ADD  SET name
    name -= expr     ->  GET name  <expr>  SUB  SET name   */
static void assignment_statement(void)
{
    advance();                                   /* identifier */
    Token name = parser.previous;
    uint8_t name_index = string_constant(name.start, (size_t)name.length);

    TokenType op = parser.current.type;          /* = += -= */
    advance();

    if (op != TOKEN_EQUAL)
        emit_bytes(OP_GET_VARIBLE, name_index);  /* push current value first */

    expression();

    if (op == TOKEN_PLUS_EQUAL)       emit_byte(OP_ADD);
    else if (op == TOKEN_MINUS_EQUAL) emit_byte(OP_SUBTRACT);

    emit_bytes(OP_SET_VARIABLE, name_index);
}

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

    else if (match(TOKEN_COLON)) {
        consume_label();
    }

    else if (match(TOKEN_JUMP)) {
        consume_jump();
    }

    else if (match(TOKEN_IF)) {
        if_statement();
    }

    else if (check(TOKEN_IDENTIFIER) && is_assign_op(peek_next())) {
        assignment_statement();
    }

    else {
        expression();
        emit_byte(OP_POP);   /* expression statement: throw the result away */
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
    reset_label_tables();          /* fresh state per compile */

    advance();
    while (parser.current.type != TOKEN_EOF) {
        statement();
        if (parser.hadError) break;
    }

    end_compiler();
    if (!parser.hadError)

        /* back-patching */
        /*
          for labels and jumps
        */
        resolve_jumps();

    reset_label_tables();
    return !parser.hadError;
}
