#include "./vm.h"

VM vm;

static bool debug_trace_execution = false;
static void reset_stack(void);


/* ------------------------------------------------------------------ */
/*  The VM loop                                                       */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/*  Truthiness + comparison helpers                                   */
/* ------------------------------------------------------------------ */

/* chars count as numbers for comparison purposes */
static bool is_comparable_number(Value v) {
    return value_is_number(v) || v.type == _1byteC;
}

/*
   truthiness:  bool -> itself,  number/char -> (value != 0)
   strings are not allowed as conditions (returns false = error)
*/
static bool value_truth(Value v, bool *out) {
    if (v.type == _bool) {
        *out = v.value._bool;
        return true;
    }
    if (is_comparable_number(v)) {
        if (value_is_float(v))
            *out = value_to_float128(v) != 0;
        else
            *out = value_to_int128(v) != 0;
        return true;
    }
    return false;
}

/*
   c = -1 : a <  b
        0 : a == b
        1 : a >  b
        2 : not comparable (NaN, or different kinds of values)

   ordered == false (==, !=): different kinds are simply "not equal" -> c = 2
   ordered == true  (<, >, <=, >=): different kinds are an error -> return false
*/
static bool compare_values(Value a, Value b, bool ordered, int *c) {

    if (is_comparable_number(a) && is_comparable_number(b)) {

        if (value_is_float(a) || value_is_float(b)) {
            __float128 x = value_to_float128(a);
            __float128 y = value_to_float128(b);

            if      (x < y)  *c = -1;
            else if (x > y)  *c = 1;
            else if (x == y) *c = 0;
            else             *c = 2;          /* NaN */
            return true;
        }

        /* integers: compare sign + magnitude so _16byteU vs negative numbers is correct */
        bool na, nb;
        __uint128_t ma, mb;
        int_parts(a, &na, &ma);
        int_parts(b, &nb, &mb);

        if (na != nb)       *c = na ? -1 : 1;
        else if (ma == mb)  *c = 0;
        else {
            int r = (ma < mb) ? -1 : 1;
            *c = na ? -r : r;
        }
        return true;
    }

    if (a.type == _string && b.type == _string) {
        int r = strcmp(a.value._string, b.value._string);
        *c = (r < 0) ? -1 : (r > 0) ? 1 : 0;
        return true;
    }

    if (!ordered) {
        if (a.type == _bool && b.type == _bool)
            *c = (a.value._bool == b.value._bool) ? 0 : 1;
        else
            *c = 2;
        return true;
    }

    return false;
}

static InterpretResult run(void) {

#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_SHORT() \
    (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))

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

#define COMPARE_OP(ordered, cond)                                           \
do {                                                                        \
    Value b = pop();                                                        \
    Value a = pop();                                                        \
    int c;                                                                  \
                                                                            \
    if (!compare_values(a, b, (ordered), &c)) {                             \
        fprintf(stderr, "Runtime error: operands must both be numbers "     \
                        "or both be strings.\n");                           \
        return INTERPRET_RUNTIME_ERROR;                                     \
    }                                                                       \
    push(_bool(cond));                                                      \
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

        case OP_EQUAL:         { COMPARE_OP(false, c == 0);               break; }
        case OP_NOT_EQUAL:     { COMPARE_OP(false, c != 0);               break; }
        case OP_GREATER:       { COMPARE_OP(true,  c == 1);               break; }
        case OP_GREATER_EQUAL: { COMPARE_OP(true,  c == 1 || c == 0);     break; }
        case OP_LESS:          { COMPARE_OP(true,  c == -1);              break; }
        case OP_LESS_EQUAL:    { COMPARE_OP(true,  c == -1 || c == 0);    break; }

        case OP_NOT: {
            bool truth;
            if (!value_truth(pop(), &truth)) {
                fprintf(stderr, "Runtime error: '!' needs a bool or a number.\n");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(_bool(!truth));
            break;
        }

        case OP_TO_BOOL: {
            bool truth;
            if (!value_truth(pop(), &truth)) {
                fprintf(stderr, "Runtime error: '&&' / '||' need bool or number operands.\n");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(_bool(truth));
            break;
        }

        /* conditional jumps peek at the top of the stack; the compiler emits the OP_POP */
        case OP_JUMP_IF_FALSE: {
            uint16_t target = READ_SHORT();
            bool truth;
            if (!value_truth(vm.stack_top[-1], &truth)) {
                fprintf(stderr, "Runtime error: condition must be a bool or a number.\n");
                return INTERPRET_RUNTIME_ERROR;
            }
            if (!truth)
                vm.ip = vm.chunk->code + target;
            break;
        }

        case OP_JUMP_IF_TRUE: {
            uint16_t target = READ_SHORT();
            bool truth;
            if (!value_truth(vm.stack_top[-1], &truth)) {
                fprintf(stderr, "Runtime error: condition must be a bool or a number.\n");
                return INTERPRET_RUNTIME_ERROR;
            }
            if (truth)
                vm.ip = vm.chunk->code + target;
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

        case OP_JUMP: {
            uint16_t target = READ_SHORT();
            vm.ip = vm.chunk->code + target;
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

        case OP_SET_VARIABLE: {

            Value name      = READ_CONSTANT();
            Value new_value = pop();
            Value current   = get_entry(vm.current_environment, name.value._string);

            /* the stored value already has the variable's declared dtype,
               so reuse it as the conversion target (range check included) */
            Value stored;
            const char *why = "";

            if (!convert_value(new_value, current.type, &stored, &why)) {
                fprintf(stderr,
                        "Runtime error: cannot assign to '%s': %s.\n",
                        name.value._string, why);

                return INTERPRET_RUNTIME_ERROR;
            }

            /* assign_entry walks up the scopes, rejects constants,
               and prints its own error message when it fails */

            if (!assign_entry(vm.current_environment, name.value._string, stored))
                return INTERPRET_RUNTIME_ERROR;

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
#undef COMPARE_OP
#undef READ_SHORT
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
