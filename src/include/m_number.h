#ifndef _M_MATH_H_
#define _M_MATH_H_

#include "fox.h"
#include "mpi2.h"

typedef struct {
    RefHeader rh;
    mp_int mp;
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
    mp_int md[2];
} RefFrac;

typedef struct {
    RefHeader rh;
    double re;
    double im;
} RefComplex;

typedef struct {
    RefHeader rh;

    int size;
    double d[0];
} RefVector;

typedef struct {
    RefHeader rh;

    int rows, cols;
    double d[0];
} RefMatrix;

#define Value_float2(v)      (((RefFloat*)Value_vp(v))->d)

#endif /* _M_MATH_H_ */
