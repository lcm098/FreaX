#ifndef FREAX_VALUE_H
#define FREAX_VALUE_H


#include "../COMMON/common.h"
#include "../MEMORY/memory.h"


/* typedef double Value; */

/*
   Keep this order: the helper functions below rely on it.
     _1byteI .. _16byteU   -> integers
     _4byteF .. _16byteF   -> floats
*/
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
    _bool,   // result of comparisons / logic (not declarable by the user yet)
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
    bool _bool;
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
#define _bool(x)    ((Value) {.type = _bool,    .value = { ._bool = (x)}})


/* ------------------------------------------------------------------ */
/*  Type helpers                                                      */
/* ------------------------------------------------------------------ */

static inline bool value_is_integer(Value v) {
    return v.type >= _1byteI && v.type <= _16byteU;
}

static inline bool value_is_float(Value v) {
    return v.type >= _4byteF && v.type <= _16byteF;
}

static inline bool value_is_number(Value v) {
    return value_is_integer(v) || value_is_float(v);
}

static inline int64_t value_to_int64(Value v) {
    switch (v.type) {
        case _1byteI:  return v.value._1byteI;
        case _1byteU:  return v.value._1byteU;
        case _2byteI:  return v.value._2byteI;
        case _2byteU:  return v.value._2byteU;
        case _4byteI:  return v.value._4byteI;
        case _4byteU:  return v.value._4byteU;
        case _8byteI:  return v.value._8byteI;
        case _8byteU:  return (int64_t)v.value._8byteU;
        case _16byteI: return (int64_t)v.value._16byteI;   /* upar ke 64 bit kat jaate hain */
        case _16byteU: return (int64_t)v.value._16byteU;
        case _4byteF:  return (int64_t)v.value._4byteF;
        case _8byteF:  return (int64_t)v.value._8byteF;
        case _16byteF: return (int64_t)v.value._16byteF;
        case _1byteC:  return v.value._1byteC;
        default:       return 0;
    }
}

static inline __int128 value_to_int128(Value v) {
    switch (v.type) {
        case _8byteU:  return (__int128)v.value._8byteU;
        case _16byteI: return v.value._16byteI;
        case _16byteU: return (__int128)v.value._16byteU;
        case _4byteF:  return (__int128)v.value._4byteF;
        case _8byteF:  return (__int128)v.value._8byteF;
        case _16byteF: return (__int128)v.value._16byteF;
        default:       return (__int128)value_to_int64(v);
    }
}

static inline __float128 value_to_float128(Value v) {
    switch (v.type) {
        case _4byteF:  return (__float128)v.value._4byteF;
        case _8byteF:  return (__float128)v.value._8byteF;
        case _16byteF: return v.value._16byteF;
        case _8byteU:  return (__float128)v.value._8byteU;
        case _16byteI: return (__float128)v.value._16byteI;
        case _16byteU: return (__float128)v.value._16byteU;
        default:       return (__float128)value_to_int64(v);
    }
}

/* ------------------------------------------------------------------ */
/*  ValueArray                                                        */
/* ------------------------------------------------------------------ */

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
