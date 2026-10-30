
#include "./vm.h"


VM vm;
static bool debug_trace_execution = false;

static InterpretResult run() {

#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define BINARY_OP(op)                                               \
do {                                                                \
    Value b = pop();                                                \
    Value a = pop();                                                \
    if (a.type == _8byteU && b.type == _8byteU) {                   \
        push(_8byteU(a.value._8byteU op b.value._8byteU));         \
    } else {                                                        \
        push(_8byteF(value_to_double(a) op value_to_double(b)));    \
    }                                                               \
} while(false)


if (debug_trace_execution) {
    printf("\n");
    printf("FreaX VIRTUAL MACHINE\n\n");
}

    for(;;) {



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
        switch(instruction = READ_BYTE()) {

        case OP_POP: {
            pop();
            break;
        }

        case OP_RETURN: {
            return INTERPRET_OK;
        }

        case OP_NEGATE: {
            Value v = pop();
            if (v.type != _8byteF) {
                fprintf(stderr, "Runtime error: can only negate a float.\n");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(_8byteF(-v.value._8byteF));
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
            BINARY_OP(/);
            break;
        }

        case OP_PRINT: {
            Value constant = pop();
            printValue(constant);
            push(constant);
            printf("\n");
            break;
        }


        case OP_CONSTANT: {
            Value value = READ_CONSTANT();
            push(value);
            break;
        }

        case OP_VARIABLE_DTYPE: {

            Value dtype = READ_CONSTANT();
            push(dtype);
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
            Value type = pop();
            Value which_type = pop();
            Value var_value = pop();

            dtype var_dtype;

            if (strcmp(type.value._string, "_8byteU") == 0) {
                var_dtype = _8byteU;
            } else if (strcmp(type.value._string, "_8byteF") == 0) {
                var_dtype = _8byteF;
            } else {
                return INTERPRET_RUNTIME_ERROR;
            }

            bool is_const = strcmp(which_type.value._string, "const") == 0;

            if (!add_entry(
                    vm.current_environment,
                    var_value,
                    variable_name.value._string,
                    is_const,
                    var_dtype
                    ))
            {
                fprintf(stderr, "Failed to define variable '%s'.\n",
                        variable_name.value._string);

                return INTERPRET_RUNTIME_ERROR;

            }

            break;
        }
    }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP

}

InterpretResult interpret(const char *source) {

    Chunk chunk;
    init_chunk(&chunk);

    if (!compiler(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();
    free_chunk(&chunk);
    return result;
}

static void reset_stack() {
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
