

#include "value.h"

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

void printValue(Value value) {
    switch (value.type) {
        case _8byteF: printf("%g",  value.value._8byteF); break;
        case _8byteU: printf("%lu", value.value._8byteU); break;
        case _string: printf("%s", value.value._string); break;
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
