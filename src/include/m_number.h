#ifndef M_NUMBER_H_INCLUDED
#define M_NUMBER_H_INCLUDED

#include "fox.h"

typedef struct {
    RefHeader rh;
    BigInt bi;
} RefInt;

typedef struct {
    RefHeader rh;
    union {
        int64_t i;
        uint64_t u;
    } u;
} RefInt64;

typedef struct {
    RefHeader rh;
    double d;
} RefFloat;

typedef struct {
    RefHeader rh;
    BigInt bi[2];
} RefFrac;

typedef struct {
    RefHeader rh;
    double re;
    double im;
} RefComplex;

typedef struct {
    RefHeader rh;

    int is_complex;
    int size;
    double d[0];
} RefVector;

typedef struct {
    RefHeader rh;

    int is_complex;
    int rows, cols;
    double d[0];
} RefMatrix;

#define Value_float2(v)      (((RefFloat*)Value_vp(v))->d)

#endif /* M_NUMBER_H_INCLUDED */
