
#ifndef FREAX_VM_H
#define FREAX_VM_H

#include "../BYTE_CODE/chunk.h"
#include "../COMMON/common.h"
#include "../DEBUG/debug.h"
#include "../VALUES/value.h"
#include "../COMPILER/compiler.h"
#include "../STORAGE/storage.h"

#define STACK_MAX 256

typedef struct {
    Chunk *chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value *stack_top;
    Environment *global_environment;
    Environment *current_environment;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

InterpretResult interpret(const char *course);

void push(Value value);
Value pop();

void init_vm();
void free_vm();



#endif
