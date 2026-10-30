
#include "chunk.h"

void init_chunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    init_value_array(&chunk->constants);
}

/*
  typedef struct {
    uint8_t *code;
    int count;
    int capacity;
  }Chunk;

*/

void write_chunk(Chunk *chunk, uint8_t byte, int line) {

    if(chunk->capacity < chunk->count+1) {

        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        int new_capacity = chunk->capacity;

        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, new_capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, old_capacity, new_capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;

    chunk->count++;
}

void free_chunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chunk(chunk);

}


/*
     * adding constants in constants pool
     * after adding returning its INDEX
*/

int add_constant(Chunk *chunk, Value value) {
    write_value_array(&chunk->constants, value);

    // return index
    return chunk->constants.count - 1;
}
