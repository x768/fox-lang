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
	mp_int md[2];
} RefFrac;

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


#endif /* _M_MATH_H_ */
