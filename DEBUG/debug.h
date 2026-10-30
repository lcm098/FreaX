
#ifndef FREAX_DEBUG_H
#define FREAX_DEBUG_H

#include "../BYTE_CODE/chunk.h"
#include "../VALUES/value.h"

void disassemble_chunk(Chunk* chunk, const char *name);
int disassemble_instruction(Chunk *chunk, int offset);

#endif
