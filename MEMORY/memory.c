
#include "../MEMORY/memory.h"

void *reallocate(void *pointer, size_t new_count) {
    if (new_count == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, new_count);
    if (result == NULL) exit(1);
    return result;
}
