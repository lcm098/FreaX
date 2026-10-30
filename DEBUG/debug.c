
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

    case OP_MUTABLE_VAR:
        return constant_instruction("OP_MUTABLE_VAR", offset, chunk);

    case OP_DEFINE_VARIABLE:
        return constant_instruction("OP_DEFINE_VARIABLE", offset, chunk);
    case OP_VARIABLE_DTYPE:
        return constant_instruction("OP_VARIABLE_DTYPE", offset, chunk);
    case OP_GET_VARIBLE:
        return constant_instruction("OP_GET_VARIBLE", offset, chunk);

    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}
