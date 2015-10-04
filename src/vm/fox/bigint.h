#ifndef _BIGINT_H_
#define _BIGINT_H_

#include "fox.h"

void BigInt_set_max_alloc(int max);

void BigInt_init(BigInt *bi);
void BigInt_close(BigInt *bi);
int BigInt_reserve(BigInt *bi, int size);

void BigInt_copy(BigInt *dst, const BigInt *src);
void int64_BigInt(BigInt *bi, int64_t value);
void uint64_BigInt(BigInt *bi, uint64_t value);
void double_BigInt(BigInt *bi, double value);
int cstr_BigInt(BigInt *bi, int radix, const char *str, int size);
int double_BigRat(BigInt *rat, double d);
int cstr_BigRat(BigInt *rat, const char *src, const char **end);
void BigRat_fix(BigInt *rat);

int32_t BigInt_int32(const BigInt *bi);
int BigInt_int64(const BigInt *bi, int64_t *value);
double BigInt_double(const BigInt *bi);
int BigInt_str_bufsize(const BigInt *bi, int radix);
int BigInt_str(const BigInt *bi, int radix, char *str, int upper);
char *BigRat_str(BigInt *rat, int max_frac);
char *BigRat_tostr_sub(int sign, BigInt *mi, BigInt *rem, int width_f);

uint32_t BigInt_hash(const BigInt *bi);
int BigInt_cmp(const BigInt *a, const BigInt *b);
int BigInt_get_recurrence(BigInt *mp, int *ret, int limit);

int BigInt_lsh(BigInt *bi, uint32_t sh);
void BigInt_rsh(BigInt *bi, uint32_t sh);
void BigInt_and(BigInt *a, const BigInt *b);
void BigInt_or(BigInt *a, const BigInt *b);
void BigInt_xor(BigInt *a, const BigInt *b);

int BigInt_add(BigInt *a, const BigInt *b);
int BigInt_add_d(BigInt *a, int b);
int BigInt_sub(BigInt *a, const BigInt *b);
int BigInt_mul(BigInt *ret, const BigInt *a, const BigInt *b);
int BigInt_mul_sd(BigInt *bi, int m);
int BigInt_divmod(BigInt *ret, BigInt *mod, const BigInt *a, const BigInt *b);
void BigInt_divmod_sd(BigInt *bi, int dv, uint16_t *mod);
int BigInt_pow(BigInt *a, uint32_t n);
int BigInt_gcd(BigInt *ret, const BigInt *a, const BigInt *b);

#endif /* _BIGINT_H_ */