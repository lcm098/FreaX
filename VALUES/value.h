
#ifndef FREAX_VALUE_H
#define FREAX_VALUE_H

#include "../COMMON/common.h"
#include "../MEMORY/memory.h"


/* typedef double Value; */

typedef enum {
    _1byteI,
    _1byteU,

    _2byteI,
    _2byteU,

    _4byteI,
    _4byteU,

    _8byteI,
    _8byteU,

    _16byteI,
    _16byteU,

    _4byteF,
    _8byteF,
    _16byteF,

    _1byteC,

    _string, // STRING NOT SUPPORTED IN LANGUAGE, STRING FOR OTHER PURPOSE
} dtype;

typedef union {
    int8_t   _1byteI;
    uint8_t  _1byteU;

    int16_t  _2byteI;
    uint16_t _2byteU;

    int32_t  _4byteI;
    uint32_t _4byteU;

    int64_t  _8byteI;
    uint64_t _8byteU;

    __int128 _16byteI;
    __uint128_t _16byteU;

    float    _4byteF;
    double   _8byteF;
    __float128 _16byteF;

    char _1byteC;

    const char *_string;
} dvalue;

typedef struct {
    dtype type;
    dvalue value;
}Value;


#define _1byteI(x)  ((Value) {.type = _1byteI,  .value = { ._1byteI = (x)}})
#define _1byteU(x)  ((Value) {.type = _1byteU,  .value = { ._1byteU = (x)}})
#define _2byteI(x)  ((Value) {.type = _2byteI,  .value = { ._2byteI = (x)}})
#define _2byteU(x)  ((Value) {.type = _2byteU,  .value = { ._2byteU = (x)}})
#define _4byteI(x)  ((Value) {.type = _4byteI,  .value = { ._4byteI = (x)}})
#define _4byteU(x)  ((Value) {.type = _4byteU,  .value = { ._4byteU = (x)}})
#define _8byteI(x)  ((Value) {.type = _8byteI,  .value = { ._8byteI = (x)}})
#define _8byteU(x)  ((Value) {.type = _8byteU,  .value = { ._8byteU = (x)}})
#define _16byteI(x) ((Value) {.type = _16byteI, .value = { ._16byteI = (x)}})
#define _16byteU(x) ((Value) {.type = _16byteU, .value = { ._16byteU = (x)}})
#define _4byteF(x)  ((Value) {.type = _4byteF,  .value = { ._4byteF = (x)}})
#define _8byteF(x)  ((Value) {.type = _8byteF,  .value = { ._8byteF = (x)}})
#define _16byteF(x) ((Value) {.type = _16byteF, .value = { ._16byteF = (x)}})
#define _1byteC(x)  ((Value) {.type = _1byteC,  .value = { ._1byteC = (x)}})
#define _string(x)  ((Value) {.type = _string,  .value = { ._string = (x)}})


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
