#ifndef _M_MATH_H_
#define _M_MATH_H_

#include "fox.h"

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
