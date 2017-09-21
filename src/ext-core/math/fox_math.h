#ifndef FOX_MATH_H_INCLUDED
#define FOX_MATH_H_INCLUDED

#include "fox.h"
#include "m_number.h"


int math_native_return_this(Value *vret, Value *v, RefNode *node);
double bytes_to_double(const uint8_t *data);
void double_to_bytes(uint8_t *data, double val);

void define_rand_func(RefNode *m);
void define_vector_func(RefNode *m);
void define_vector_class(RefNode *m);


#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_math;
extern RefNode *cls_complex;
extern RefNode *cls_vector;
extern RefNode *cls_matrix;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


#endif /* FOX_MATH_H_INCLUDED */
