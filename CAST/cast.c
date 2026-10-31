#include "./cast.h"

const DtypeName dtype_names[] = {
    {"_1byteI",  _1byteI},   {"_1byteU",  _1byteU},
    {"_2byteI",  _2byteI},   {"_2byteU",  _2byteU},
    {"_4byteI",  _4byteI},   {"_4byteU",  _4byteU},
    {"_8byteI",  _8byteI},   {"_8byteU",  _8byteU},
    {"_16byteI", _16byteI},  {"_16byteU", _16byteU},
    {"_4byteF",  _4byteF},   {"_8byteF",  _8byteF},
    {"_16byteF", _16byteF},
    {"_1byteC",  _1byteC},
    {"_string",  _string},
};

bool dtype_from_name(const char *name, dtype *out)
{
    for (size_t i = 0; i < sizeof dtype_names / sizeof dtype_names[0]; i++) {
        if (strcmp(dtype_names[i].name, name) == 0) {
            *out = dtype_names[i].type;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  Value conversion to a declared datatype (with range check)        */
/* ------------------------------------------------------------------ */

/* integer target: kitne bits, signed hai ya nahi */
bool int_target_info(dtype t, int *bits, bool *is_signed)
{
    switch (t) {
        case _1byteI:
        case _1byteC:  *bits = 8;   *is_signed = true;  return true;
        case _1byteU:  *bits = 8;   *is_signed = false; return true;
        case _2byteI:  *bits = 16;  *is_signed = true;  return true;
        case _2byteU:  *bits = 16;  *is_signed = false; return true;
        case _4byteI:  *bits = 32;  *is_signed = true;  return true;
        case _4byteU:  *bits = 32;  *is_signed = false; return true;
        case _8byteI:  *bits = 64;  *is_signed = true;  return true;
        case _8byteU:  *bits = 64;  *is_signed = false; return true;
        case _16byteI: *bits = 128; *is_signed = true;  return true;
        case _16byteU: *bits = 128; *is_signed = false; return true;
        default:       return false;
    }
}

void int_parts(Value v, bool *neg, __uint128_t *mag)
{
    if (v.type == _16byteU) {
        *neg = false;
        *mag = v.value._16byteU;
        return;
    }

    __int128 s = value_to_int128(v);
    *neg = s < 0;
    *mag = *neg ? (__uint128_t)0 - (__uint128_t)s : (__uint128_t)s;
}

bool fits_int(bool neg, __uint128_t mag, int bits, bool is_signed)
{
    if (!is_signed) {
        if (neg) return false;
        __uint128_t max = (bits == 128) ? ~(__uint128_t)0
                                        : (((__uint128_t)1) << bits) - 1;
        return mag <= max;
    }

    __uint128_t limit = ((__uint128_t)1) << (bits - 1);     /* 2^(bits-1) */
    return neg ? (mag <= limit) : (mag <= limit - 1);
}

bool convert_value(Value in, dtype target, Value *out, const char **why)
{
    /* ---- string target: only a string value fits ---- */
    if (target == _string) {
        if (in.type != _string) {
            *why = "a _string variable can only hold a string value";
            return false;
        }
        *out = in;
        return true;
    }

    /* ---- numeric targets: a string value is not allowed ---- */
    if (in.type == _string) {
        *why = "cannot store a string in a non-string variable";
        return false;
    }

    if (!value_is_number(in)) {
        *why = "value is not a number";
        return false;
    }

    /* ---- float targets ---- */
    if (target == _4byteF || target == _8byteF || target == _16byteF) {
        __float128 f = value_to_float128(in);

        if (target == _4byteF) {
            if (f > FLT_MAX || f < -FLT_MAX) { *why = "value out of range for _4byteF"; return false; }
            *out = _4byteF((float)f);
        } else if (target == _8byteF) {
            if (f > DBL_MAX || f < -DBL_MAX) { *why = "value out of range for _8byteF"; return false; }
            *out = _8byteF((double)f);
        } else {
            *out = _16byteF(f);
        }
        return true;
    }

    /* ---- integer / char targets ---- */
    if (value_is_float(in)) {
        *why = "cannot store a decimal number in an integer variable";
        return false;
    }

    int bits;
    bool is_signed;
    if (!int_target_info(target, &bits, &is_signed)) {
        *why = "unknown target datatype";
        return false;
    }

    bool neg;
    __uint128_t mag;
    int_parts(in, &neg, &mag);

    if (!fits_int(neg, mag, bits, is_signed)) {
        *why = "value out of range for this datatype";
        return false;
    }

    __uint128_t raw = neg ? (__uint128_t)0 - mag : mag;      /* two's complement bits */

    switch (target) {
        case _1byteI:  *out = _1byteI((int8_t)raw);    break;
        case _1byteU:  *out = _1byteU((uint8_t)raw);   break;
        case _2byteI:  *out = _2byteI((int16_t)raw);   break;
        case _2byteU:  *out = _2byteU((uint16_t)raw);  break;
        case _4byteI:  *out = _4byteI((int32_t)raw);   break;
        case _4byteU:  *out = _4byteU((uint32_t)raw);  break;
        case _8byteI:  *out = _8byteI((int64_t)raw);   break;
        case _8byteU:  *out = _8byteU((uint64_t)raw);  break;
        case _16byteI: *out = _16byteI((__int128)raw); break;
        case _16byteU: *out = _16byteU(raw);           break;
        case _1byteC:  *out = _1byteC((char)raw);      break;
        default:       *why = "unknown target datatype"; return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  Arithmetic helpers                                                */
/* ------------------------------------------------------------------ */

/* integer jo int64 me safe nahi aata -> 128-bit math chahiye */
bool needs_int128(Value v)
{
    return v.type == _8byteU || v.type == _16byteI || v.type == _16byteU;
}
