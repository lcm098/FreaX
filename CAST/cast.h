
#ifndef FREAX_CAST_H
#define FREAX_CAST_H

#include "../VALUES/value.h"
#include "../COMMON/common.h"

/* ------------------------------------------------------------------ */
/*  dtype name  ->  dtype                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    dtype type;
} DtypeName;

extern const DtypeName dtype_names[];


bool dtype_from_name(const char *name, dtype *out);
bool int_target_info(dtype t, int *bits, bool *is_signed);
void int_parts(Value v, bool *neg, __uint128_t *mag);
bool fits_int(bool neg, __uint128_t mag, int bits, bool is_signed);
bool convert_value(Value in, dtype target, Value *out, const char **why);
bool needs_int128(Value v);


#endif
