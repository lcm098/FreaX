#include "value.h"

#include <inttypes.h>
#include <quadmath.h>
#include <stdio.h>

/*
typedef struct {
    int count;
    int capacity;
    Value *values;
} ValueArray;
*/

void init_value_array(ValueArray* array) {
    array->values = NULL;
    array->capacity=0;
    array->count=0;
}

/* ---------- 128-bit integer printing (printf ke paas %lld ka 128-bit version nahi) ---------- */

static void print_uint128(__uint128_t n) {
    char buf[40];
    int i = 39;
    buf[i] = '\0';

    if (n == 0) {
        putchar('0');
        return;
    }

    while (n > 0) {
        buf[--i] = (char)('0' + (int)(n % 10));
        n /= 10;
    }
    fputs(&buf[i], stdout);
}

static void print_int128(__int128 n) {
    if (n < 0) {
        putchar('-');
        print_uint128((__uint128_t)0 - (__uint128_t)n);
    } else {
        print_uint128((__uint128_t)n);
    }
}

void printValue(Value value) {
    switch (value.type) {
        case _1byteI:
            printf("%" PRId8, value.value._1byteI);
            break;

        case _1byteU:
            printf("%" PRIu8, value.value._1byteU);
            break;

        case _2byteI:
            printf("%" PRId16, value.value._2byteI);
            break;

        case _2byteU:
            printf("%" PRIu16, value.value._2byteU);
            break;

        case _4byteI:
            printf("%" PRId32, value.value._4byteI);
            break;

        case _4byteU:
            printf("%" PRIu32, value.value._4byteU);
            break;

        case _8byteI:
            printf("%" PRId64, value.value._8byteI);
            break;

        case _8byteU:
            printf("%" PRIu64, value.value._8byteU);
            break;

        case _16byteI:
            print_int128(value.value._16byteI);
            break;

        case _16byteU:
            print_uint128(value.value._16byteU);
            break;

        case _4byteF:
            printf("%g", (double)value.value._4byteF);
            break;

        case _8byteF:
            printf("%g", value.value._8byteF);
            break;

        case _16byteF: {
            char buf[128];
            quadmath_snprintf(buf, sizeof buf, "%Qg", value.value._16byteF);
            printf("%s", buf);
            break;
        }

        case _1byteC:
            printf("%c", value.value._1byteC);
            break;

        case _string:
            printf("%s", value.value._string);
            break;

        case _bool:
            printf("%s", value.value._bool ? "true" : "false");
            break;
    }
}

void write_value_array(ValueArray* array, Value value) {

    if (array->capacity < array->count+1) {

        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        int new_capacity = array->capacity;

        array->values = GROW_ARRAY(Value, array->values, old_capacity, new_capacity);
    }

    array->values[array->count] = value;
    array->count++;
}


void free_value_array(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    init_value_array(array);
}
