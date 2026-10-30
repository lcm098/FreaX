
#ifndef FREAX_VALUE_H
#define FREAX_VALUE_H

#include "../COMMON/common.h"
#include "../MEMORY/memory.h"


/* typedef double Value; */

typedef enum {
    _8byteF,
    _8byteU,
    _string,
} dtype;

typedef union {
    double _8byteF;
    unsigned long int _8byteU;
    const char* _string;
} dvalue;

typedef struct {
    dtype type;
    dvalue value;
}Value;

#define _8byteF(x) ((Value) {.type = _8byteF, .value = { ._8byteF = (x)}})
#define _8byteU(x) ((Value) {.type = _8byteU, .value = { ._8byteU = (x)}})
#define _string(x) ((Value) {.type = _string, .value = { ._string = (x)}})



static inline double value_to_double(Value v) {
    return (v.type == _8byteF) ? v.value._8byteF : (double)v.value._8byteU;
}

static inline unsigned long int value_to_unsigned_long_int(Value v) {
    return (v.type == _8byteU) ? v.value._8byteU : (unsigned long int)v.value._8byteF;
}

typedef struct {
    int count;
    int capacity;
    Value *values;
} ValueArray;

void init_value_array(ValueArray* array);
void write_value_array(ValueArray* array, Value value);
void free_value_array(ValueArray* array);
void printValue(Value value);


#endif
