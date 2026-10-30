
#ifndef FREAX_COMPILE_H
#define FREAX_COMPILE_H

#include "../COMMON/common.h"
#include "../SCANNER/scanner.h"
#include "../BYTE_CODE/chunk.h"

bool compiler(const char *source, Chunk *chunk);

#endif
