#include "./vm.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VM vm;
static bool debug_trace_execution = false;

static void reset_stack(void);

/* ------------------------------------------------------------------ */
/*  dtype name  ->  dtype                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    dtype type;
} DtypeName;

static const DtypeName dtype_names[] = {
    {"_1byteI",  _1byteI},   {"_1byteU",  _1byteU},
    {"_2byteI",  _2byteI},   {"_2byteU",  _2byteU},
    {"_4byteI",  _4byteI},   {"_4byteU",  _4byteU},
    {"_8byteI",  _8byteI},   {"_8byteU",  _8byteU},
    {"_16byteI", _16byteI},  {"_16byteU", _16byteU},
    {"_4byteF",  _4byteF},   {"_8byteF",  _8byteF},   {"_16byteF", _16byteF},
    {"_1byteC",  _1byteC},
};

/* "_8byteU" jaisi string -> dtype enum. Nahi mila to false */
static bool dtype_from_name(const char *name, dtype *out)
{
    for (size_t i = 0; i < sizeof dtype_names / sizeof dtype_names[0]; i++) {
        if (strcmp(dtype_names[i].name, name) == 0) {
            *out = dtype_names[i].type;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  Value conversion to a declared datatype (with range check)        */
/* ------------------------------------------------------------------ */

/* integer target: kitne bits, signed hai ya nahi */
static bool int_target_info(dtype t, int *bits, bool *is_signed)
{
    switch (t) {
        case _1byteI:
        case _1byteC:  *bits = 8;   *is_signed = true;  return true;
        case _1byteU:  *bits = 8;   *is_signed = false; return true;
        case _2byteI:  *bits = 16;  *is_signed = true;  return true;
        case _2byteU:  *bits = 16;  *is_signed = false; return true;
        case _4byteI:  *bits = 32;  *is_signed = true;  return true;
        case _4byteU:  *bits = 32;  *is_signed = false; return true;
        case _8byteI:  *bits = 64;  *is_signed = true;  return true;
        case _8byteU:  *bits = 64;  *is_signed = false; return true;
        case _16byteI: *bits = 128; *is_signed = true;  return true;
        case _16byteU: *bits = 128; *is_signed = false; return true;
        default:       return false;
    }
}

/* integer value -> (negative?, magnitude): _16byteU ki poori range handle hoti hai */
static void int_parts(Value v, bool *neg, __uint128_t *mag)
{
    if (v.type == _16byteU) {
        *neg = false;
        *mag = v.value._16byteU;
        return;
    }

    __int128 s = value_to_int128(v);
    *neg = s < 0;
    *mag = *neg ? (__uint128_t)0 - (__uint128_t)s : (__uint128_t)s;
}

static bool fits_int(bool neg, __uint128_t mag, int bits, bool is_signed)
{
    if (!is_signed) {
        if (neg) return false;
        __uint128_t max = (bits == 128) ? ~(__uint128_t)0
                                        : (((__uint128_t)1) << bits) - 1;
        return mag <= max;
    }

    __uint128_t limit = ((__uint128_t)1) << (bits - 1);     /* 2^(bits-1) */
    return neg ? (mag <= limit) : (mag <= limit - 1);
}

/* in ko target dtype me badlo (range check ke saath). false => *why me wajah */
static bool convert_value(Value in, dtype target, Value *out, const char **why)
{
    if (!value_is_number(in)) {
        *why = "value is not a number";
        return false;
    }

    /* ---- float targets ---- */
    if (target == _4byteF || target == _8byteF || target == _16byteF) {
        __float128 f = value_to_float128(in);

        if (target == _4byteF) {
            if (f > FLT_MAX || f < -FLT_MAX) { *why = "value out of range for _4byteF"; return false; }
            *out = _4byteF((float)f);
        } else if (target == _8byteF) {
            if (f > DBL_MAX || f < -DBL_MAX) { *why = "value out of range for _8byteF"; return false; }
            *out = _8byteF((double)f);
        } else {
            *out = _16byteF(f);
        }
        return true;
    }

    /* ---- integer / char targets ---- */
    if (value_is_float(in)) {
        *why = "cannot store a decimal number in an integer variable";
        return false;
    }

    int bits;
    bool is_signed;
    if (!int_target_info(target, &bits, &is_signed)) {
        *why = "unknown target datatype";
        return false;
    }

    bool neg;
    __uint128_t mag;
    int_parts(in, &neg, &mag);

    if (!fits_int(neg, mag, bits, is_signed)) {
        *why = "value out of range for this datatype";
        return false;
    }

    __uint128_t raw = neg ? (__uint128_t)0 - mag : mag;      /* two's complement bits */

    switch (target) {
        case _1byteI:  *out = _1byteI((int8_t)raw);    break;
        case _1byteU:  *out = _1byteU((uint8_t)raw);   break;
        case _2byteI:  *out = _2byteI((int16_t)raw);   break;
        case _2byteU:  *out = _2byteU((uint16_t)raw);  break;
        case _4byteI:  *out = _4byteI((int32_t)raw);   break;
        case _4byteU:  *out = _4byteU((uint32_t)raw);  break;
        case _8byteI:  *out = _8byteI((int64_t)raw);   break;
        case _8byteU:  *out = _8byteU((uint64_t)raw);  break;
        case _16byteI: *out = _16byteI((__int128)raw); break;
        case _16byteU: *out = _16byteU(raw);           break;
        case _1byteC:  *out = _1byteC((char)raw);      break;
        default:       *why = "unknown target datatype"; return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  Arithmetic helpers                                                */
/* ------------------------------------------------------------------ */

/* integer jo int64 me safe nahi aata -> 128-bit math chahiye */
static bool needs_int128(Value v)
{
    return v.type == _8byteU || v.type == _16byteI || v.type == _16byteU;
}

/* ------------------------------------------------------------------ */
/*  The VM loop                                                       */
/* ------------------------------------------------------------------ */

static InterpretResult run(void) {

#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

/*
   Promotion rules:
     any _16byteF           -> _16byteF
     any other float        -> _8byteF
     any _8byteU/_16byte int-> _16byteI
     otherwise              -> _8byteI
   (integer overflow of the 64/128-bit result is not checked yet)
*/
#define BINARY_OP(op)                                                       \
do {                                                                        \
    Value b = pop();                                                        \
    Value a = pop();                                                        \
                                                                            \
    if (!value_is_number(a) || !value_is_number(b)) {                       \
        fprintf(stderr, "Runtime error: operands must be numbers.\n");      \
        return INTERPRET_RUNTIME_ERROR;                                     \
    }                                                                       \
                                                                            \
    if (a.type == _16byteF || b.type == _16byteF) {                         \
        push(_16byteF(value_to_float128(a) op value_to_float128(b)));       \
    } else if (value_is_float(a) || value_is_float(b)) {                    \
        push(_8byteF((double)value_to_float128(a)                           \
                     op (double)value_to_float128(b)));                     \
    } else if (needs_int128(a) || needs_int128(b)) {                        \
        push(_16byteI(value_to_int128(a) op value_to_int128(b)));           \
    } else {                                                                \
        push(_8byteI(value_to_int64(a) op value_to_int64(b)));              \
    }                                                                       \
} while (false)


    if (debug_trace_execution) {
        printf("\n");
        printf("FreaX VIRTUAL MACHINE\n\n");
    }

    for (;;) {

        if (debug_trace_execution) {
            printf("STACK=[\n");
            for (Value* slot = vm.stack_top; slot > vm.stack; ) {
                slot--;
                printf("\tSLOT=[");
                printValue(*slot);
                printf("]\n");
            }
            printf("]\n");

            disassemble_instruction(
                vm.chunk,
                (int)(vm.ip - vm.chunk->code)
            );
        }

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {

        case OP_POP: {
            pop();
            break;
        }

        case OP_RETURN: {
            return INTERPRET_OK;
        }

        case OP_NEGATE: {
            Value v = pop();

            if (v.type == _16byteF) {
                push(_16byteF(-v.value._16byteF));

            } else if (value_is_float(v)) {
                push(_8byteF(-(double)value_to_float128(v)));

            } else if (value_is_integer(v)) {

                if (needs_int128(v))
                    push(_16byteI(-value_to_int128(v)));
                else
                    push(_8byteI(-value_to_int64(v)));

            } else {

                fprintf(stderr, "Runtime error: can only negate a number.\n");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }

        case OP_DEBUG_ON: {
            debug_trace_execution = true;
            break;
        }

        case OP_DEBUG_OFF: {
            debug_trace_execution = false;
            break;
        }

        case OP_ADD: {
            BINARY_OP(+);
            break;
        }

        case OP_SUBTRACT: {
            BINARY_OP(-);
            break;
        }

        case OP_MULTIPLY: {
            BINARY_OP(*);
            break;
        }

        case OP_DIVIDE: {
            /* integer / 0 hardware crash (SIGFPE) deta hai, isliye pehle check */
            Value b = vm.stack_top[-1];
            Value a = vm.stack_top[-2];

            if (value_is_integer(a) && value_is_integer(b) &&
                value_to_int128(b) == 0) {
                fprintf(stderr, "Runtime error: division by zero.\n");
                return INTERPRET_RUNTIME_ERROR;
            }

            BINARY_OP(/);
            break;
        }

        case OP_PRINT: {
            /* print value ko consume karta hai (stack me wapas push nahi karta,
               warna har console statement ke saath stack badhta jayega) */
            Value value = pop();
            printValue(value);
            printf("\n");
            break;
        }

        case OP_CONSTANT: {
            Value value = READ_CONSTANT();
            push(value);
            break;
        }

        case OP_VARIABLE_DTYPE: {
            Value dtype_name = READ_CONSTANT();
            push(dtype_name);
            break;
        }

        case OP_CONSTANT_VAR: {
            Value which_type = READ_CONSTANT();
            push(which_type);
            break;
        }

        case OP_MUTABLE_VAR: {
            Value which_type = READ_CONSTANT();
            push(which_type);
            break;
        }

        case OP_BEGIN_SCOPE: {
            Environment *environment = new_environment(vm.current_environment);
            if (environment == NULL)
                return INTERPRET_RUNTIME_ERROR;
            vm.current_environment = environment;
            break;
        }

        case OP_END_SCOPE: {
            if (vm.current_environment == vm.global_environment) {
                fprintf(stderr, "Runtime error: cannot close the global scope.\n");
                return INTERPRET_RUNTIME_ERROR;
            }

            Environment *enclosing = vm.current_environment->enclosing;
            destroy_environment(vm.current_environment);
            vm.current_environment = enclosing;
            break;
        }

        case OP_GET_VARIBLE: {
            Value var_name = READ_CONSTANT();
            Value var_value = get_entry(vm.current_environment, var_name.value._string);
            push(var_value);
            break;
        }

        case OP_DEFINE_VARIABLE: {

            Value variable_name = READ_CONSTANT();
            Value type          = pop();   /* top:    "_1byteU", "_8byteF", ... */
            Value which_type    = pop();   /* middle: "mutable" / "constant"    */
            Value var_value     = pop();   /* bottom: expression ka result      */

            dtype var_dtype;

            if (!dtype_from_name(type.value._string, &var_dtype)) {
                fprintf(stderr, "Runtime error: unknown datatype '%s'.\n",
                        type.value._string);
                return INTERPRET_RUNTIME_ERROR;
            }

            Value stored;
            const char *why = "";

            if (!convert_value(var_value, var_dtype, &stored, &why)) {
                fprintf(stderr,
                        "Runtime error: cannot store value in '%s' variable '%s': %s.\n",
                        type.value._string, variable_name.value._string, why);
                return INTERPRET_RUNTIME_ERROR;
            }

            bool is_const = strcmp(which_type.value._string, "constant") == 0;

            if (!add_entry(vm.current_environment,
                           stored,
                           variable_name.value._string,
                           is_const,
                           var_dtype))
            {
                fprintf(stderr, "Failed to define variable '%s'.\n",
                        variable_name.value._string);
                return INTERPRET_RUNTIME_ERROR;
            }

            break;
        }

        default: {
            fprintf(stderr, "Runtime error: unknown opcode %d.\n", instruction);
            return INTERPRET_RUNTIME_ERROR;
        }

        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP

}

/* ------------------------------------------------------------------ */
/*  Entry points                                                      */
/* ------------------------------------------------------------------ */

InterpretResult interpret(const char *source) {

    Chunk chunk;
    init_chunk(&chunk);

    if (!compiler(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    reset_stack();
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    while (vm.current_environment != vm.global_environment) {
        Environment *enclosing = vm.current_environment->enclosing;
        destroy_environment(vm.current_environment);
        vm.current_environment = enclosing;
    }

    reset_stack();
    free_chunk(&chunk);
    return result;
}

static void reset_stack(void) {
    vm.stack_top = vm.stack;
}

void push(Value value) {
    *vm.stack_top = value;
    vm.stack_top++;
}

Value pop() {
    vm.stack_top--;
    return *vm.stack_top;
}

void init_vm() {
    reset_stack();
    vm.global_environment = new_environment(NULL);
    vm.current_environment = vm.global_environment;
}

void free_vm() {
    reset_stack();
    destroy_environment(vm.global_environment);
    vm.global_environment = NULL;
    vm.current_environment = NULL;
}
