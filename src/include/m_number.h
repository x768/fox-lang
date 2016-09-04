#ifndef _M_NUMBER_H_
#define _M_NUMBER_H_

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

    int size;
    double d[0];
} RefVector;

typedef struct {
    RefHeader rh;

    int rows, cols;
    double d[0];
} RefMatrix;

#define Value_float2(v)      (((RefFloat*)Value_vp(v))->d)

#endif /* _M_NUMBER_H_ */
