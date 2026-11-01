#ifndef FREAX_CHUNK_H
#define FREAX_CHUNK_H

#include "../COMMON/common.h"
#include "../MEMORY/memory.h"
#include "../VALUES/value.h"

typedef enum {

    // GENRAL OP_CODES
    OP_RETURN,
    OP_NEGATE,
    OP_PRINT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_DEBUG_ON,
    OP_DEBUG_OFF,
    OP_CONSTANT,
    OP_POP,
    OP_BEGIN_SCOPE,
    OP_END_SCOPE,
    OP_LABEL,
    OP_JUMP,
    OP_JUMP_IF_FALSE,         // jump if top of stack is falsey  (does NOT pop)
    OP_JUMP_IF_TRUE,          // jump if top of stack is truthy  (does NOT pop)

    // COMPARISON / LOGIC
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_NOT,                   // !x
    OP_TO_BOOL,               // turn top of stack into a bool (used by && and ||)

    // ===== ARRAY (new) =====
    OP_ARRAY_NEW,             // dtype-const, ndims, ndims x u32 dim : push a new zero-filled array (MULTIDIM changed)
    OP_ARRAY_INIT,            // u32 flat-index : pop value, store it at that leaf position (array stays on stack) (MULTIDIM changed)
    OP_DEFINE_ARRAY,          // name-const : pop mutability + array, define the variable
    OP_ARRAY_GET,             // pop index, pop array, push element
    OP_ARRAY_SET,             // pop value, pop index, pop array, store element
    OP_DUP2,                  // duplicate the top two stack values (for a[i] += x)
    // ===== END ARRAY =====

    // VARIABLE
    OP_DEFINE_VARIABLE,
    OP_GET_VARIBLE,
    OP_SET_VARIABLE,
    OP_VARIABLE_DTYPE,
    OP_CONSTANT_VAR,
    OP_MUTABLE_VAR,

    // POINTER CREATION
    OP_PTR_NULL,              // Push a null pointer.
    OP_PTR_ADDR_LOCAL,        // Push the address of a local variable.
    OP_PTR_ADDR_GLOBAL,       // Push the address of a global variable.
    OP_PTR_ADDR_INDEX,        // Push the address of an indexed element.

    OP_PTR_DEFINE_LOCAL,       // define new local pointer
    OP_PTR_DEFINE_GLOBAL,      // define new global pointer

    // POINTER READ/WRITE
    OP_PTR_DEREF,             // Read a value from the pointed memory.
    OP_PTR_STORE_INDIRECT,    // Write a value to the pointed memory.

    // POINTER ARITHMETIC
    OP_PTR_ADD,               // Advance a pointer by an element offset.
    OP_PTR_SUBTRACT,          // Move a pointer backward by an element offset.
    OP_PTR_DIFFERENCE,        // Calculate the distance between two pointers.

    // POINTER COMPARISON
    OP_PTR_EQUAL,             // Check whether two pointers are equal.
    OP_PTR_NOT_EQUAL,         // Check whether two pointers are different.
    OP_PTR_LESS,              // Check whether one pointer is less than another.
    OP_PTR_LESS_EQUAL,        // Check whether one pointer is less than or equal to another.
    OP_PTR_GREATER,           // Check whether one pointer is greater than another.
    OP_PTR_GREATER_EQUAL,     // Check whether one pointer is greater than or equal to another.

    // POINTER CONVERSION
    OP_PTR_CAST,              // Convert a pointer to another pointer type.

    // POINTER MEMORY ALLOCATION AND RELEASE
    OP_PTR_ALLOC,             // Allocate uninitialized heap memory.
    OP_PTR_ALLOC_ZEROED,      // Allocate heap memory initialized to zero.
    OP_PTR_REALLOC,           // Resize an existing heap allocation.
    OP_PTR_FREE,              // Release previously allocated heap memory.

    // POINTER MEMORY OPERATIONS
    OP_PTR_MEM_COPY,          // Copy bytes between memory regions.
    OP_PTR_MEM_MOVE,          // Move bytes safely between overlapping regions.
    OP_PTR_MEM_SET,           // Fill a memory region with a byte value.
    OP_PTR_MEM_COMPARE,       // Compare two memory regions byte by byte.

    // POINTER AND MEMORY VALIDATION
    OP_PTR_VALIDATE,          // Check whether a pointer is valid to use.
    OP_PTR_IS_NULL,           // Check whether a pointer is null.

}OpCode;

typedef struct {
    uint8_t *code;
    int count;
    int capacity;
    int *lines;
    ValueArray constants;
}Chunk;

void init_chunk(Chunk *chunk);
void write_chunk(Chunk *chunk, uint8_t byte, int line);
void free_chunk(Chunk *chunk);
int add_constant(Chunk *chunk, Value value);

#endif
