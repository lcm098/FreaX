
#ifndef FREAX_MEMORY_H
#define FREAX_MEMORY_H

#include "../COMMON/common.h"

#define GROW_CAPACITY(capacity)              \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, old_count, new_count)                                 \
    (type *) reallocate(pointer, sizeof(type) * (new_count))

#define FREE_ARRAY(type, pointer, oldCount)              \
    reallocate(pointer, 0)

void *reallocate(void *pointer, size_t new_count);

#endif
