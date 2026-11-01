#include "debug.h"

void disassemble_chunk(Chunk* chunk, const char *name) {
    printf("=============== %s ===============\n", name);

    for (int offset=0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

static int simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}


static int constant_instruction(const char* name, int offset, Chunk *chunk) {

    printf("%s ", name);
    int index = chunk->code[offset+1];

    Value constant = chunk->constants.values[index];
    printValue(constant);
    printf("\n");

    return offset + 2;
}

/* (jumps step) opcode + 2-byte absolute target address, high byte first */
static int jump_instruction(const char* name, int offset, Chunk *chunk) {
    uint16_t target = (uint16_t)((chunk->code[offset + 1] << 8) |
                                  chunk->code[offset + 2]);
    printf("%s -> %05d\n", name, target);
    return offset + 3;
}


int disassemble_instruction(Chunk *chunk, int offset) {

    printf("%05d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("| ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch(instruction) {

    case OP_RETURN:
        return simple_instruction("OP_RETURN", offset);

    case OP_CONSTANT:
        return constant_instruction("OP_CONSTANT", offset, chunk);

    case OP_NEGATE:
        return simple_instruction("OP_NEGATE", offset);

    case OP_POP:
        return simple_instruction("OP_POP", offset);

    case OP_PRINT:
        return simple_instruction("OP_PRINT", offset);

    case OP_DEBUG_ON:
        return simple_instruction("OP_DEBUG_ON", offset);

    case OP_ADD:
      return simple_instruction("OP_ADD", offset);

    case OP_SUBTRACT:
      return simple_instruction("OP_SUBTRACT", offset);

    case OP_MULTIPLY:
      return simple_instruction("OP_MULTIPLY", offset);

    case OP_DIVIDE:
      return simple_instruction("OP_DIVIDE", offset);

    case OP_BEGIN_SCOPE:
        return simple_instruction("OP_BEGIN_SCOPE", offset);

    case OP_END_SCOPE:
        return simple_instruction("OP_END_SCOPE", offset);

    case OP_DEBUG_OFF:
        return simple_instruction("OP_DEBUG_OFF", offset);

    case OP_CONSTANT_VAR:
        return constant_instruction("OP_CONSTANT_VAR", offset, chunk);

    /* (jumps step) OP_JUMP now has a 2-byte address operand */
    case OP_JUMP:
        return jump_instruction("OP_JUMP", offset, chunk);

    case OP_MUTABLE_VAR:
        return constant_instruction("OP_MUTABLE_VAR", offset, chunk);

    case OP_DEFINE_VARIABLE:
        return constant_instruction("OP_DEFINE_VARIABLE", offset, chunk);
    case OP_VARIABLE_DTYPE:
        return constant_instruction("OP_VARIABLE_DTYPE", offset, chunk);
    case OP_GET_VARIBLE:
        return constant_instruction("OP_GET_VARIBLE", offset, chunk);

    /* (assignment step) */
    case OP_SET_VARIABLE:
        return constant_instruction("OP_SET_VARIABLE", offset, chunk);

    /* (if/else + comparison + logic step) */
    case OP_JUMP_IF_FALSE:
        return jump_instruction("OP_JUMP_IF_FALSE", offset, chunk);

    case OP_JUMP_IF_TRUE:
        return jump_instruction("OP_JUMP_IF_TRUE", offset, chunk);

    case OP_EQUAL:
        return simple_instruction("OP_EQUAL", offset);

    case OP_NOT_EQUAL:
        return simple_instruction("OP_NOT_EQUAL", offset);

    case OP_GREATER:
        return simple_instruction("OP_GREATER", offset);

    case OP_GREATER_EQUAL:
        return simple_instruction("OP_GREATER_EQUAL", offset);

    case OP_LESS:
        return simple_instruction("OP_LESS", offset);

    case OP_LESS_EQUAL:
        return simple_instruction("OP_LESS_EQUAL", offset);

    case OP_NOT:
        return simple_instruction("OP_NOT", offset);

    case OP_TO_BOOL:
        return simple_instruction("OP_TO_BOOL", offset);

    /* ===== ARRAY (new) ===== */
    /* ===== MULTIDIM (new/changed) ===== */
    case OP_ARRAY_NEW: {
        Value type = chunk->constants.values[chunk->code[offset + 1]];
        int ndims  = chunk->code[offset + 2];

        printf("OP_ARRAY_NEW type=%s dims=", type.value._string);
        for (int d = 0; d < ndims; d++) {
            int at = offset + 3 + 4 * d;
            uint32_t dim = ((uint32_t)chunk->code[at]     << 24) |
                           ((uint32_t)chunk->code[at + 1] << 16) |
                           ((uint32_t)chunk->code[at + 2] << 8)  |
                            (uint32_t)chunk->code[at + 3];
            printf("[%u]", dim);
        }
        printf("\n");
        return offset + 3 + 4 * ndims;
    }

    case OP_ARRAY_INIT: {
        uint32_t leaf = ((uint32_t)chunk->code[offset + 1] << 24) |
                        ((uint32_t)chunk->code[offset + 2] << 16) |
                        ((uint32_t)chunk->code[offset + 3] << 8)  |
                         (uint32_t)chunk->code[offset + 4];
        printf("OP_ARRAY_INIT leaf=%u\n", leaf);
        return offset + 5;
    }
    /* ===== END MULTIDIM ===== */

    case OP_DEFINE_ARRAY:
        return constant_instruction("OP_DEFINE_ARRAY", offset, chunk);

    case OP_ARRAY_GET:
        return simple_instruction("OP_ARRAY_GET", offset);

    case OP_ARRAY_SET:
        return simple_instruction("OP_ARRAY_SET", offset);

    case OP_DUP2:
        return simple_instruction("OP_DUP2", offset);
    /* ===== END ARRAY ===== */

    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}
