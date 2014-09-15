#include "fox.h"
#include "m_math.h"
#include "mpi.h"
#include "mpi2.h"
#include "mplogic.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>


enum {
	ROUND_DOWN,
	ROUND_UP,
	ROUND_FLOOR,
	ROUND_CEILING,
	ROUND_HALF_DOWN,
	ROUND_HALF_UP,
	ROUND_HALF_EVEN,
};

typedef struct {
	RefHeader rh;
	mp_int mp;
} RefInt;

typedef struct {
	RefHeader rh;
	mp_int md[2];
} RefFrac;


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_math;
static RefNode *cls_vector;
static RefNode *cls_matrix;


/*
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote
        products derived from this software without specific prior written
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

// Period parameters
enum {
	N = 624,
	M = 397,
	MATRIX_A = 0x9908b0dfUL,   // constant vector a
	UPPER_MASK = 0x80000000UL, // most significant w-r bits
	LOWER_MASK = 0x7fffffffUL, // least significant r bits
};

static uint32_t mt[N];                // the array for the state vector
static int mti = N + 1;              // mti==N+1 means mt[N] is not initialized
static int rand_initialized = FALSE;

/**
 * initializes mt[N] with a seed
 */
static void init_genrand(uint32_t s)
{
	mt[0]= s & 0xffffffffUL;
	for (mti = 1; mti < (int)N; mti++) {
		mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
		/* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
		/* In the previous versions, MSBs of the seed affect   */
		/* only MSBs of the array mt[].                        */
		/* 2002/01/09 modified by Makoto Matsumoto             */
	}
}
/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
static void init_by_array(const uint32_t *init_key, int key_length)
{
	int i = 1;
	int j = 0;
	int k;

	init_genrand(19650218UL);

	for (k = ((int)N > key_length ? N : key_length); k != 0; k--) {
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525UL)) + init_key[j] + j; // non linear
		i++;
		j++;
		if (i >= (int)N) {
			mt[0] = mt[N - 1];
			i = 1;
		}
		if (j >= key_length) {
			j = 0;
		}
	}
	for (k = N - 1; k != 0; k--) {
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941UL)) - i; // non linear
		i++;
		if (i >= (int)N) {
			mt[0] = mt[N - 1];
			i = 1;
		}
	}
	mt[0] = 0x80000000UL; // MSB is 1; assuring non-zero initial array
}

/**
 * generates a random number on [0,0xffffffff]-interval
 */
static uint32_t gen_int32()
{
	static uint32_t mag01[2] = {0x0UL, MATRIX_A};
	/* mag01[x] = x * MATRIX_A  for x=0,1 */

	uint32_t y;

	if (mti >= (int)N) { /* generate N words at one time */
		int kk;

		if (mti == (int)(N + 1))   /* if init_genrand() has not been called, */
			init_genrand(5489UL); /* a default initial seed is used */

		for (kk = 0; kk < (int)(N - M); kk++) {
			y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
			mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		for (; kk < (int)(N - 1); kk++) {
			y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
			mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
		}
		y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
		mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];
		mti = 0;
	}
	y = mt[mti++];

	/* Tempering */
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);

	return y;
}

/**
 * 乱数を初期化
 */
static void init_rand48()
{
	enum {
		LEN = 64,
	};
	uint32_t buf[LEN];
	fs->get_random(buf, LEN * sizeof(uint32_t));
	init_by_array(buf, LEN);

	rand_initialized = TRUE;
}

uint32_t get_rand_uint32()
{
	if (!rand_initialized) {
		init_rand48();
	}
	return gen_int32();
}

static int math_rand_seed(Value *vret, Value *v, RefNode *node)
{
	if (fg->stk_top > v + 1) {
		Value v1 = v[1];
		const RefNode *v1_type = fs->Value_type(v1);
		char *c_buf;
		int c_buf_len;

		if (v1_type == fs->cls_int) {
			if (Value_isref(v1)) {
				mp_int *mp = Value_vp(*v);
				c_buf_len = (mp->used + 1) / 2;
				c_buf = malloc(c_buf_len * 4);
				memcpy(c_buf, mp->dp, mp->used * 2);
				if ((mp->used % 2) != 0) {
					memset(c_buf + mp->used * 2, 0, 1);
				}
			} else {
				uint32_t ival = Value_integral(v1);
				c_buf_len = 1;
				c_buf = malloc(c_buf_len * 4);
				memcpy(c_buf, &ival, 4);
			}
		} else if (v1_type == fs->cls_str || v1_type == fs->cls_bytes) {
			Str s = fs->Value_str(v1);
			c_buf_len = (s.size + 3) / 4;
			c_buf = malloc(c_buf_len * 4);
			memcpy(c_buf, s.p, s.size);
			if ((s.size % 4) != 0) {
				memset(c_buf + s.size, 0, c_buf_len * 4 - s.size);
			}
		} else {
			fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_int, fs->cls_sequence, v1_type, 1);
			return FALSE;
		}
		init_by_array((const uint32_t*)c_buf, c_buf_len);
		rand_initialized = TRUE;
		free(c_buf);
	} else {
		init_rand48();
	}

	return TRUE;
}
/**
 * 引数で指定したサイズのBytesを返す
 * 内容はランダムビット
 */
static int math_rand_bin(Value *vret, Value *v, RefNode *node)
{
	int n, i;
	RefStr *rs;
	Value v1 = v[1];
	int64_t size;
	int err = FALSE;

	if (!rand_initialized) {
		init_rand48();
	}

	size = fs->Value_int(v1, &err);
	if (size < 0) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Must be >= 0 (argument #1)");
		return FALSE;
	}
	if (err || size > fs->max_alloc) {
		fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
		return FALSE;
	}
	n = (size + 3) / 4;

	rs = fs->new_refstr_n(fs->cls_bytes, n * 4);
	*vret = vp_Value(rs);
	for (i = 0; i < n; i++) {
		uint32_t r = gen_int32();
		memcpy(rs->c + i * 4, &r, 4);
	}
	rs->c[size] = '\0';
	rs->size = size;

	return TRUE;
}
static int math_rand_int(Value *vret, Value *v, RefNode *node)
{
	Value v1 = v[1];

	if (!rand_initialized) {
		init_rand48();
	}
	if (Value_isref(v1)) {
		mp_int mp, q;
		int i, n;
		mp_int *mpval = Value_vp(v1);
		RefInt *rem;

		if (mpval->sign == MP_NEG) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Must be > 0 (argument #1)");
			return FALSE;
		}
		n = (mp_count_bits(mpval) / 32) + 1;

		mp_init(&mp);
		for (i = 0; i < n; i++) {
			uint32_t r = gen_int32();

			mpl_lsh(&mp, &mp, 16);
			mp_add_d(&mp, r & 0xFFFF, &mp);
			mpl_lsh(&mp, &mp, 16);
			mp_add_d(&mp, r >> 16, &mp);
		}
		mp_init(&q);
		rem = fs->new_buf(fs->cls_int, sizeof(RefInt));
		*vret = vp_Value(rem);
		mp_init(&rem->mp);
		mp_div(&mp, mpval, &q, &rem->mp);
		mp_clear(&mp);
		mp_clear(&q);

		fs->fix_bigint(vret, &rem->mp);
	} else {
		int32_t lval = Value_integral(v1);

		if (lval <= 0) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Must be > 0 (argument #1)");
			return FALSE;
		}
		// lvalがgen_int32の返す最大値に近いと、偏りが生じる
		if (lval < 65536) {
			int32_t ret = gen_int32() % lval;
			*vret = int32_Value(ret);
		} else {
			uint64_t val = ((uint64_t)gen_int32() << 32) | ((uint64_t)gen_int32());
			int32_t ret = val % lval;
			*vret = int32_Value(ret);
		}
	}

	return TRUE;
}
static int math_rand_float(Value *vret, Value *v, RefNode *node)
{
	double dval;

	if (!rand_initialized) {
		init_rand48();
	}
	dval = (double)gen_int32() / 4294967296.0;
	*vret = fs->float_Value(dval);

	return TRUE;
}
/**
 * This is described in section 3.4.1 of The Art of Computer
 * Programming, Volume 2 by Donald Knuth.
 */
static int math_rand_gaussian(Value *vret, Value *v, RefNode *node)
{
	static int has_next;
	static double next_val;
	double dval;

	if (has_next) {
		dval = next_val;
		has_next = FALSE;
	} else {
	    double v1, v2, s, norm;

	    if (!rand_initialized) {
			init_rand48();
		}
		do {
			v1 = ((double)gen_int32() / 2147483648.0) - 1.0; // Between -1.0 and 1.0.
			v2 = ((double)gen_int32() / 2147483648.0) - 1.0; // Between -1.0 and 1.0.
			s = v1 * v1 + v2 * v2;
		} while (s >= 1.0);

		norm = sqrt(-2 * log(s) / s);
		next_val = v2 * norm;
		has_next = TRUE;
		dval = v1 * norm;
	}
	*vret = fs->float_Value(dval);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int math_func1(Value *vret, Value *v, RefNode *node)
{
	double (*fn)(double) = FUNC_VP(node);
	double arg = Value_float2(v[1]);
	double ret;

	errno = 0;
	ret = fn(arg);
	if (errno != 0) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "%s", strerror(errno));
		return FALSE;
	}
	if (isnan(ret) || isinf(ret)) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal argument value");
		return FALSE;
	}
	*vret = fs->float_Value(ret);

	return TRUE;
}
static int math_func2(Value *vret, Value *v, RefNode *node)
{
	double (*fn)(double, double) = FUNC_VP(node);
	double arg1 = Value_float2(v[1]);
	double arg2 = Value_float2(v[2]);
	double ret;

	errno = 0;
	ret = fn(arg1, arg2);
	if (errno != 0) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "%s", strerror(errno));
		return FALSE;
	}
	if (isnan(ret) || isinf(ret)) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal argument value");
		return FALSE;
	}
	*vret = fs->float_Value(ret);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int vector_new(Value *vret, Value *v, RefNode *node)
{
	Value *vv = v + 1;
	int size = (fg->stk_top - v) - 1;
	int i;
	RefVector *vec;

	if (size == 1) {
		if (fs->Value_type(v[1]) == fs->cls_list) {
			RefArray *ra = Value_vp(v[1]);
			vv = ra->p;
			size = ra->size;
		}
	}
	vec = fs->new_buf(cls_vector, sizeof(RefVector) + sizeof(double) * size);
	*vret = vp_Value(vec);
	vec->size = size;

	for (i = 0; i < size; i++) {
		const RefNode *v_type = fs->Value_type(vv[i]);
		if (v_type != fs->cls_float && v_type != fs->cls_int && v_type != fs->cls_frac) {
			fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_number, v_type, i + 1);
			return FALSE;
		}
		vec->d[i] = fs->Value_float(vv[i]);
	}

	return TRUE;
}
static int vector_dup(Value *vret, Value *v, RefNode *node)
{
	RefVector *src = Value_vp(*v);
	int size = sizeof(RefVector) + sizeof(double) * src->size;
	RefVector *dst = fs->new_buf(cls_vector, size);
	*vret = vp_Value(dst);
	memcpy(dst->d, src->d, size);

	return TRUE;
}
static int vector_size(Value *vret, Value *v, RefNode *node)
{
	RefVector *vec = Value_vp(*v);
	*vret = int32_Value(vec->size);
	return TRUE;
}
/**
 * 要素がすべて0.0ならtrue
 * 要素数が0ならtrue
 */
static int vector_empty(Value *vret, Value *v, RefNode *node)
{
	RefVector *vec = Value_vp(*v);
	int i;
	for (i = 0; i < vec->size; i++) {
		if (vec->d[0] != 0.0) {
			*vret = VALUE_FALSE;
			return TRUE;
		}
	}
	*vret = VALUE_TRUE;
	return TRUE;
}
static int vector_norm(Value *vret, Value *v, RefNode *node)
{
	double norm = 0.0;
	int i;
	RefVector *vec = Value_vp(*v);

	for (i = 0; i < vec->size; i++) {
		double d = vec->d[i];
		norm += d * d;
	}
	norm = sqrt(norm);
	*vret = fs->float_Value(norm);

	return TRUE;
}
static int vector_to_matrix(Value *vret, Value *v, RefNode *node)
{
	int i;
	RefVector *vec = Value_vp(*v);
	RefMatrix *mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * vec->size);
	*vret = vp_Value(mat);

	if (FUNC_INT(node)) {
		// 縦
		mat->cols = 1;
		mat->rows = vec->size;
	} else {
		// 横
		mat->cols = vec->size;
		mat->rows = 1;
	}
	for (i = 0; i < vec->size; i++) {
		mat->d[i] = vec->d[i];
	}

	return TRUE;
}
static int vector_tostr(Value *vret, Value *v, RefNode *node)
{
	StrBuf sb;
	RefVector *vec = Value_vp(*v);
	int i;

	fs->StrBuf_init_refstr(&sb, 0);
	fs->StrBuf_add(&sb, "Vector(", 7);

	for (i = 0; i < vec->size; i++) {
		char cbuf[64];
		if (i > 0) {
			fs->StrBuf_add(&sb, ", ", 2);
		}
		sprintf(cbuf, "%g", vec->d[i]);
		fs->StrBuf_add(&sb, cbuf, -1);
	}
	fs->StrBuf_add_c(&sb, ')');
	*vret = fs->StrBuf_str_Value(&sb, fs->cls_str);

	return TRUE;
}
static int vector_hash(Value *vret, Value *v, RefNode *node)
{
	RefVector *vec = Value_vp(*v);
	uint32_t *i32 = (uint32_t*)vec->d;
	int i, size;
	uint32_t hash = 0;

	size = vec->size * 2;   // sizeof(double) / sizeof(uint32_t)
	for (i = 0; i < size; i++) {
		hash ^= i32[i];
	}
	*vret = int32_Value(hash & INT32_MAX);

	return TRUE;
}
static int vector_eq(Value *vret, Value *v, RefNode *node)
{
	RefVector *v1 = Value_vp(v[0]);
	RefVector *v2 = Value_vp(v[1]);

	if (v1->size == v2->size) {
		int i;
		for (i = 0; i < v1->size; i++) {
			if (v1->d[i] != v2->d[i]) {
				break;
			}
		}
		if (i < v1->size) {
			*vret = VALUE_FALSE;
		} else {
			*vret = VALUE_TRUE;
		}
	} else {
		*vret = VALUE_FALSE;
	}
	return TRUE;
}
static int vector_index(Value *vret, Value *v, RefNode *node)
{
	RefVector *vec = Value_vp(*v);
	int err = FALSE;
	int64_t i = fs->Value_int(v[1], &err);

	if (i < 0) {
		i += vec->size;
	}
	if (!err && i >= 0 && i < vec->size) {
		*vret = fs->float_Value(vec->d[i]);
	} else {
		fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], vec->size);
		return FALSE;
	}
	return TRUE;
}
static int vector_index_set(Value *vret, Value *v, RefNode *node)
{
	RefVector *vec = Value_vp(*v);
	int err = FALSE;
	int64_t i = fs->Value_int(v[1], &err);

	if (i < 0) {
		i += vec->size;
	}
	if (i >= 0 && i < vec->size) {
		vec->d[i] = fs->Value_float(v[i]);
	} else {
		fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], vec->size);
		return FALSE;
	}
	return TRUE;
}
static int vector_addsub(Value *vret, Value *v, RefNode *node)
{
	double factor = (FUNC_INT(node) ? -1.0 : 1.0);
	RefVector *v1 = Value_vp(v[0]);
	RefVector *v2 = Value_vp(v[1]);
	int size, i;
	RefVector *vec;

	if (v1->size != v2->size) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "RefVector size mismatch");
		return FALSE;
	}
	size = v1->size;
	vec = fs->new_buf(cls_vector, sizeof(RefVector) + sizeof(double) * size);
	*vret = vp_Value(vec);
	vec->size = size;

	for (i = 0; i < size; i++) {
		vec->d[i] = v1->d[i] + v2->d[i] * factor;
	}

	return TRUE;
}
/**
 * 各要素をx倍する
 */
static int vector_muldiv(Value *vret, Value *v, RefNode *node)
{
	RefVector *v1 = Value_vp(v[0]);
	double d = fs->Value_float(v[1]);
	int i;
	RefVector *vec;

	if (FUNC_INT(node)) {
		d = 1.0 / d;
	}

	vec = fs->new_buf(cls_vector, sizeof(RefVector) + sizeof(double) * v1->size);
	*vret = vp_Value(vec);
	vec->size = v1->size;

	for (i = 0; i < v1->size; i++) {
		vec->d[i] = v1->d[i] * d;
	}

	return TRUE;
}
static int matrix_new(Value *vret, Value *v, RefNode *node)
{
	Value *vv = v + 1;
	int size = (fg->stk_top - v) - 1;
	int cols = -1;
	int i;
	RefMatrix *mat;

	// 引数が配列1つで、その配列の最初の要素も配列の場合
	if (size == 1) {
		if (fs->Value_type(v[1]) == fs->cls_list) {
			RefArray *ra = Value_vp(v[1]);
			if (ra->size > 0 && fs->Value_type(ra->p[0]) == fs->cls_list) {
				vv = ra->p;
				size = ra->size;
			}
		}
	}
	for (i = 0; i < size; i++) {
		const RefNode *v_type = fs->Value_type(vv[i]);
		RefArray *ra;

		if (v_type != fs->cls_list) {
			fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_list, v_type, i + 1);
			return FALSE;
		}
		ra = Value_vp(vv[i]);
		if (cols == -1) {
			cols = ra->size;
		} else {
			if (cols != ra->size) {
				fs->throw_errorf(fs->mod_lang, "ValueError", "Array length mismatch");
				return FALSE;
			}
		}
	}
	mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size * cols);
	*vret = vp_Value(mat);
	mat->rows = size;
	mat->cols = cols;

	for (i = 0; i < size; i++) {
		RefArray *ra = Value_vp(vv[i]);
		int j;
		
		for (j = 0; j < cols; j++) {
			const RefNode *v_type = fs->Value_type(ra->p[j]);
			if (v_type != fs->cls_float && v_type != fs->cls_int && v_type != fs->cls_frac) {
				fs->throw_errorf(fs->mod_lang, "TypeError", "Number required but %n", v_type);
				return FALSE;
			}
			mat->d[i * cols + j] = fs->Value_float(ra->p[j]);
		}
	}

	return TRUE;
}
/*
 * 単位行列
 */
static int matrix_i(Value *vret, Value *v, RefNode *node)
{
	int64_t size = fs->Value_int(v[1], NULL);
	RefMatrix *mat;
	double *p;

	// 0x0の行列も可能
	if (size < 0 || size > 16384) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal Matrix size (0 - 16384)");
		return FALSE;
	}

	mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size * size);
	*vret = vp_Value(mat);
	mat->rows = size;
	mat->cols = size;
	p = mat->d;

	{
		int i, j;
		for (i = 0; i < size; i++) {
			for (j = 0; j < size; j++) {
				*p = (i == j ? 1.0 : 0.0);
				p++;
			}
		}
	}

	return TRUE;
}
/*
 * 0行列
 */
static int matrix_zero(Value *vret, Value *v, RefNode *node)
{
	int64_t size1 = fs->Value_int(v[1], NULL);
	int64_t size2 = fs->Value_int(v[2], NULL);
	RefMatrix *mat;
	double *p;

	// 0x0の行列も可能
	if (size1 < 0 || size1 > 16384 || size2 < 0 || size2 > 16384) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal Matrix size (0 - 16384)");
		return FALSE;
	}

	mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size1 * size2);
	*vret = vp_Value(mat);
	mat->cols = size1;
	mat->rows = size2;
	p = mat->d;

	{
		int size = size1 * size2;
		int i;
		for (i = 0; i < size; i++) {
			*p = 0.0;
			p++;
		}
	}

	return TRUE;
}
static int matrix_dup(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *src = Value_vp(*v);
	int size = sizeof(RefMatrix) + sizeof(double) * src->cols * src->rows;
	RefMatrix *dst = fs->new_buf(cls_matrix, size);
	*vret = vp_Value(dst);
	memcpy(dst->d, src->d, size);

	return TRUE;
}
static int matrix_size(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *mat = Value_vp(*v);
	int ret;
	if (FUNC_INT(node)) {
		ret = mat->cols;
	} else {
		ret = mat->rows;
	}
	*vret = int32_Value(ret);
	return TRUE;
}
static int matrix_to_vector(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *mat = Value_vp(*v);
	RefVector *vec;
	int idx = fs->Value_int(v[1], NULL);
	int i;

	if (FUNC_INT(node)) {
		if (idx < 0) {
			idx += mat->cols;
		}
		if (idx < 0 || idx >= mat->cols) {
			fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, &v[1], mat->cols);
			return FALSE;
		}
		vec = fs->new_buf(cls_vector, sizeof(RefVector) + sizeof(double) * mat->rows);
		*vret = vp_Value(vec);
		vec->size = mat->rows;
		for (i = 0; i < mat->rows; i++) {
			vec->d[i] = mat->d[i * mat->cols + idx];
		}
	} else {
		if (idx < 0) {
			idx += mat->rows;
		}
		if (idx < 0 || idx >= mat->rows) {
			fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, &v[1], mat->rows);
			return FALSE;
		}
		vec = fs->new_buf(cls_vector, sizeof(RefVector) + sizeof(double) * mat->cols);
		*vret = vp_Value(vec);
		vec->size = mat->cols;
		for (i = 0; i < mat->cols; i++) {
			vec->d[i] = mat->d[idx * mat->cols + i];
		}
	}
	return TRUE;
}
static int matrix_empty(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *mat = Value_vp(*v);
	int i;
	int size = mat->cols * mat->rows;

	for (i = 0; i < size; i++) {
		if (mat->d[i] != 0.0) {
			*vret = VALUE_FALSE;
			return TRUE;
		}
	}
	*vret = VALUE_TRUE;
	return TRUE;
}
/*
 * 転置行列
 */
static int matrix_transpose(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *mat = Value_vp(*v);
	RefMatrix *mat2;
	int i, j;
	int rows = mat->rows;
	int cols = mat->cols;

	mat2 = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * rows * cols);
	*vret = vp_Value(mat2);
	mat2->rows = cols;
	mat2->cols = rows;

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			mat2->d[j * rows + i] = mat->d[i * cols + j];
		}
	}

	return TRUE;
}
static int matrix_eq(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *m1 = Value_vp(v[0]);
	RefMatrix *m2 = Value_vp(v[1]);
	int rows = m1->rows;
	int cols = m1->cols;
	int i, size;

	if (rows != m2->rows || cols != m2->cols) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Matrix size mismatch");
		return FALSE;
	}
	size = rows * cols;

	for (i = 0; i < size; i++) {
		if (m1->d[i] != m2->d[i]) {
			*vret = VALUE_FALSE;
			return TRUE;
		}
	}
	*vret = VALUE_TRUE;

	return TRUE;
}
static int matrix_addsub(Value *vret, Value *v, RefNode *node)
{
	double factor = (FUNC_INT(node) ? -1.0 : 1.0);
	RefMatrix *m1 = Value_vp(v[0]);
	RefMatrix *m2 = Value_vp(v[1]);
	int size, i;
	RefMatrix *mat;

	if (m1->rows != m2->rows || m1->cols != m2->cols) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Matrix size mismatch");
		return FALSE;
	}
	size = m1->rows * m1->cols;
	mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size);
	*vret = vp_Value(mat);
	mat->rows = m1->rows;
	mat->cols = m1->cols;

	for (i = 0; i < size; i++) {
		mat->d[i] = m1->d[i] + m2->d[i] * factor;
	}

	return TRUE;
}
/**
 * 右辺が数値の場合、各要素をx倍する
 * 右辺がRefMatrixの場合、行列の積を求める
 */
static int matrix_multiple(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *m1 = Value_vp(v[0]);
	const RefNode *v1_type = fs->Value_type(v[1]);
	int cols1 = m1->cols;
	int rows1 = m1->rows;

	if (v1_type == cls_matrix) {
		RefMatrix *m2 = Value_vp(v[1]);
		RefMatrix *mat;
		int cols2 = m2->cols;
		int rows2 = m2->rows;
		int i, j, k;

		if (cols1 != rows2) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix size mismatch");
			return FALSE;
		}
		mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * rows1 * cols2);
		*vret = vp_Value(mat);
		mat->rows = rows1;
		mat->cols = cols2;
		
		for (i = 0; i < rows1; i++) {
			for (j = 0; j < cols2; j++) {
				double sum = 0.0;
				for (k = 0; k < cols1; k++) {
					sum += m1->d[i * cols1 + k] * m2->d[k * cols2 + j];
				}
				mat->d[i * cols2 + j] = sum;
			}
		}
	} else if (v1_type == fs->cls_float || v1_type == fs->cls_int || v1_type == fs->cls_frac) {
		int i;
		double d2 = fs->Value_float(v[1]);
		int size = cols1 * rows1;
		RefMatrix *mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size);
		*vret = vp_Value(mat);

		mat->cols = cols1;
		mat->rows = rows1;
		for (i = 0; i < size; i++) {
			mat->d[i] = m1->d[i] * d2;
		}
	} else {
		fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, cls_matrix, fs->cls_number, v1_type, 1);
		return FALSE;
	}

	return TRUE;
}
static double get_matrix_norm(RefMatrix *mat)
{
	double norm = 0.0;
	int i;
	int size = mat->cols * mat->rows;

	for (i = 0; i < size; i++) {
		double d = mat->d[i];
		norm += d * d;
	}
	return sqrt(norm);
}
static int matrix_norm(Value *vret, Value *v, RefNode *node)
{
	double norm = get_matrix_norm(Value_vp(*v));
	*vret = fs->float_Value(norm);
	return TRUE;
}

static int matrix_index(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *mat = Value_vp(*v);
	int64_t i = fs->Value_int(v[1], NULL);  // 行
	int64_t j = fs->Value_int(v[2], NULL);  // 列
	if (i < 0) {
		i += mat->rows;
	}
	if (j < 0) {
		j += mat->cols;
	}
	if (i >= 0 && i < mat->rows && j >= 0 && j < mat->cols) {
		*vret = fs->float_Value(mat->d[i * mat->cols + j]);
	} else {
		fs->throw_errorf(fs->mod_lang, "IndexError", "Invalid matrix index ((%v, %v) of (%d, %d))", v[1], v[2], mat->rows, mat->cols);
		return FALSE;
	}
	return TRUE;
}
static int matrix_index_set(Value *vret, Value *v, RefNode *node)
{
	RefMatrix *mat = Value_vp(*v);
	int64_t i = fs->Value_int(v[1], NULL);  // 行
	int64_t j = fs->Value_int(v[2], NULL);  // 列
	if (i < 0) {
		i += mat->rows;
	}
	if (j < 0) {
		j += mat->cols;
	}
	if (i >= 0 && i < mat->rows && j >= 0 && j < mat->cols) {
		mat->d[i * mat->cols + j] = fs->Value_float(v[3]);
	} else {
		fs->throw_errorf(fs->mod_lang, "IndexError", "Invalid matrix index ((%v, %v) of (%d, %d))", v[1], v[2], mat->rows, mat->cols);
		return FALSE;
	}
	return TRUE;
}
static int matrix_tostr(Value *vret, Value *v, RefNode *node)
{
	StrBuf sb;
	RefMatrix *mat = Value_vp(*v);
	int cols = mat->cols;
	int size = cols * mat->rows;
	Str fmt;
	int i;

	if (fg->stk_top > v + 1) {
		fmt = fs->Value_str(v[1]);
	} else {
		fmt.p = NULL;
		fmt.size = 0;
	}

	if (Str_eq_p(fmt, "p") || Str_eq_p(fmt, "pretty")) {
		// 各列の横幅の最大値を求める
		uint8_t *max_w = malloc(cols);
		int rows = mat->rows;

		fs->StrBuf_init_refstr(&sb, 0);

		for (i = 0; i < cols; i++) {
			// i : 列
			int j;
			int max = 0;
			for (j = 0; j < rows; j++) {
				// j : 行
				char cbuf[64];
				int len;
				sprintf(cbuf, "%g", mat->d[j * cols + i]);
				len = strlen(cbuf);
				if (max < len) {
					max = len;
				}
			}
			max_w[i] = max;
		}

		for (i = 0; i < rows; i++) {
			// i : 行
			int j;
			fs->StrBuf_add_c(&sb, '|');
			for (j = 0; j < cols; j++) {
				// j : 列
				char cbuf[64];
				sprintf(cbuf, " %*g ", max_w[j], mat->d[i * cols + j]);
				fs->StrBuf_add(&sb, cbuf, -1);
			}
			fs->StrBuf_add(&sb, "|\n", 2);
		}

		free(max_w);

		*vret = fs->StrBuf_str_Value(&sb, fs->cls_str);
	} else if (fmt.size == 0) {
		fs->StrBuf_init_refstr(&sb, 0);
		fs->StrBuf_add(&sb, "Matrix([", 8);

		for (i = 0; i < size; i++) {
			char cbuf[64];
			if (i > 0) {
				if (i % cols == 0) {
					fs->StrBuf_add(&sb, "], [", 4);
				} else {
					fs->StrBuf_add(&sb, ", ", 2);
				}
			}
			sprintf(cbuf, "%g", mat->d[i]);
			fs->StrBuf_add(&sb, cbuf, -1);
		}
		fs->StrBuf_add(&sb, "])", 2);

		*vret = fs->StrBuf_str_Value(&sb, fs->cls_str);
	} else {
		fs->throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", fmt);
		return FALSE;
	}

	return TRUE;
}
static int matrix_hash(Value *vret, Value *v, RefNode *node)
{
	int i, size;
	RefMatrix *mat = Value_vp(*v);
	uint32_t *i32 = (uint32_t*)mat->d;
	long hash = 0;

	size = mat->rows * mat->cols * 2;   // sizeof(double) / sizeof(uint32_t)

	for (i = 0; i < size; i++) {
		hash ^= i32[i];
	}
	*vret = int32_Value(hash & INT32_MAX);

	return TRUE;
}
static int math_dot(Value *vret, Value *v, RefNode *node)
{
	RefVector *v1 = Value_vp(v[1]);
	RefVector *v2 = Value_vp(v[2]);
	double d = 0.0;
	int i, n;

	if (v1->size != v2->size) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "RefVector size mismatch");
		return FALSE;
	}
	n = v1->size;
	for (i = 0; i < n; i++) {
		d += v1->d[i] * v2->d[i];
	}
	*vret = fs->float_Value(d);

	return TRUE;
}
static int math_cross(Value *vret, Value *v, RefNode *node)
{
	RefVector *v1 = Value_vp(v[1]);
	RefVector *v2 = Value_vp(v[2]);
	RefVector *vr;

	if (v1->size != 3 || v2->size != 3) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "RefVector size must be 3");
		return FALSE;
	}
	vr = fs->new_buf(cls_vector, sizeof(RefVector) + sizeof(double) * 3);
	*vret = vp_Value(vr);
	vr->size = 3;

	vr->d[0] = v1->d[1] * v2->d[2] - v1->d[2] * v2->d[1];
	vr->d[1] = v1->d[2] * v2->d[0] - v1->d[0] * v2->d[2];
	vr->d[2] = v1->d[0] * v2->d[1] - v1->d[1] * v2->d[0];

	return TRUE;
}
static int math_outer(Value *vret, Value *v, RefNode *node)
{
	int x, y;
	RefVector *v1 = Value_vp(v[1]);
	RefVector *v2 = Value_vp(v[2]);
	int rows = v1->size;
	int cols = v2->size;

	RefMatrix *mat = fs->new_buf(cls_matrix, sizeof(RefMatrix) + sizeof(double) * v1->size * v2->size);
	*vret = vp_Value(mat);

	mat->rows = rows;
	mat->cols = cols;

	for (y = 0; y < rows; y++) {
		for (x = 0; x < cols; x++) {
			mat->d[x + y * cols] = v1->d[y] + v2->d[x];
		}
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////

/**
 * 負の数なら、正の数に変換して返す
 * 正の数なら、そのまま返す
 */
static int math_abs(Value *vret, Value *v, RefNode *node)
{
	Value v1 = v[1];
	const RefNode *v_type = fs->Value_type(v1);

	if (v_type == fs->cls_int) {
		// Int
		if (Value_isref(v1)) {
			RefInt *mp = Value_vp(v1);
			if (mp->mp.sign == MP_NEG) {
				RefInt *mp2 = fs->new_buf(fs->cls_int, sizeof(RefInt));
				*vret = vp_Value(mp2);
				mp_init(&mp2->mp);
				mp_abs(&mp->mp, &mp2->mp);
			} else {
				// move 参照カウンタは変わらない
				*vret = v1;
				v[1] = VALUE_NULL;
			}
		} else {
			int32_t ival = Value_integral(v1);
			if (ival < 0) {
				ival = -ival;
			}
			*vret = int32_Value(ival);
		}
	} else if (v_type == fs->cls_float) {
		// Float
		double dval = Value_float2(v1);
		if (dval < 0.0) {
			dval = -dval;
		}
		*vret = fs->float_Value(dval);
	} else {
		// Rational
		RefFrac *md = Value_vp(v1);

		if (md->md[0].sign == MP_NEG) {
			RefFrac *md2 = fs->new_buf(fs->cls_frac, sizeof(RefFrac));
			*vret = vp_Value(md2);
			mp_init(&md2->md[0]);
			mp_init(&md2->md[1]);
			mp_abs(&md->md[0], &md2->md[0]);
			mp_copy(&md->md[1], &md2->md[1]);
		} else {
			// move 参照カウンタは変わらない
			*vret = v1;
			v[1] = VALUE_NULL;
		}
	}

	return TRUE;
}
/**
 * 最大公約数を返す
 */
static int math_gcd(Value *vret, Value *v, RefNode *node)
{
	Value v1 = v[1];
	Value v2 = v[2];

	if (Value_isref(v1) || Value_isref(v2)) {
		mp_int m;
		mp_int n;
		RefInt *gcd;

		mp_init(&m);
		mp_init(&n);
		if (Value_isref(v1)) {
			RefInt *r1 = Value_vp(v1);
			m = r1->mp;
		} else {
			mp_set_int(&m, Value_integral(v1));
		}
		if (Value_isref(v2)) {
			RefInt *r2 = Value_vp(v2);
			n = r2->mp;
		} else {
			mp_set_int(&n, Value_integral(v2));
		}

		if (m.sign == MP_NEG || n.sign == MP_NEG) {
			if (!Value_isref(v1)) {
				mp_clear(&m);
			}
			if (!Value_isref(v2)) {
				mp_clear(&n);
			}
			goto ERROR_END;
		}

		gcd = fs->new_buf(fs->cls_int, sizeof(RefInt));
		*vret = vp_Value(gcd);
		mp_init(&gcd->mp);
		mp_gcd(&m, &n, &gcd->mp);
		fs->fix_bigint(vret, &gcd->mp);

		if (!Value_isref(v1)) {
			mp_clear(&m);
		}
		if (!Value_isref(v2)) {
			mp_clear(&n);
		}
	} else {
		int32_t m = Value_integral(v1);
		int32_t n = Value_integral(v2);

		if (m <= 0 || n <= 0) {
			goto ERROR_END;
		}
		if (m < n) {
			int tmp = m;
			m = n;
			n = tmp;
		}
		while (n > 0) {
			int tmp = m % n;
			m = n;
			n = tmp;
		}
		*vret = int32_Value(m);
	}

	return TRUE;

ERROR_END:
	fs->throw_errorf(fs->mod_lang, "ValueError", "Parameters must be positive value");
	return FALSE;
}
static int math_round(Value *vret, Value *v, RefNode *node)
{
	int type = FUNC_INT(node);
	Value v1 = v[1];
	RefFrac *md = Value_vp(v1);
	RefFrac *mp;   // result
	mp_int mul, rem;

	int sign = md->md[0].sign;
	int64_t pred = 0;
	if (fg->stk_top > v + 2) {
		pred = fs->Value_int(v[2], NULL);
		if (pred < -32768 || pred > 32767) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal parameter #2 (-32768 - 32767)");
			return FALSE;
		}
	}

	mp = fs->new_buf(fs->cls_frac, sizeof(RefFrac));
	*vret = vp_Value(mp);
	mp_init(&mp->md[0]);
	mp_init(&mp->md[1]);

	mp_abs(&md->md[0], &mp->md[0]);
	mp_copy(&md->md[1], &mp->md[1]);
	if (type == ROUND_FLOOR) {
		if (sign == MP_NEG) {
			type = ROUND_UP;
		} else {
			type = ROUND_DOWN;
		}
	} else if (type == ROUND_CEILING) {
		if (sign == MP_NEG) {
			type = ROUND_DOWN;
		} else {
			type = ROUND_UP;
		}
	}

	mp_init(&mul);
	if (pred < 0) {
		mp_set(&mul, 10);
		mp_expt_d(&mul, -pred, &mul);
		mp_mul(&mp->md[1], &mul, &mp->md[1]);
	} else if (pred > 0) {
		mp_set(&mul, 10);
		mp_expt_d(&mul, pred, &mul);
		mp_mul(&mp->md[0], &mul, &mp->md[0]);
	}

	mp_init(&rem);
	mp_div(&mp->md[0], &mp->md[1], &mp->md[0], &rem);

	switch (type) {
	case ROUND_DOWN:
		break;
	case ROUND_UP:
		if (mp_cmp_z(&rem) > 0) {
			mp_add_d(&mp->md[0], 1, &mp->md[0]);
		}
		break;
	case ROUND_HALF_DOWN:
		mp_mul_d(&rem, 2, &rem);
		if (mp_cmp(&rem, &mp->md[1]) > 0) {
			mp_add_d(&mp->md[0], 1, &mp->md[0]);
		}
		break;
	case ROUND_HALF_UP:
		mp_mul_d(&rem, 2, &rem);
		if (mp_cmp(&rem, &mp->md[1]) >= 0) {
			mp_add_d(&mp->md[0], 1, &mp->md[0]);
		}
		break;
	case ROUND_HALF_EVEN: {
		int cmp;
		mp_mul_d(&rem, 2, &rem);
		cmp = mp_cmp(&rem, &mp->md[1]);
		if (cmp > 0 || (cmp == 0 && mp_isodd(&mp->md[0]))) {
			mp_add_d(&mp->md[0], 1, &mp->md[0]);
		}
		break;
	}
	}
	if (pred < 0) {
		mp_mul(&mp->md[0], &mul, &mp->md[0]);
		mp_set(&mp->md[1], 1);
	} else if (pred > 0) {
		mp_copy(&mul, &mp->md[1]);
	} else {
		mp_set(&mp->md[1], 1);
	}
	mp_clear(&mul);
	mp_clear(&rem);

	mp2_fix_rational(&mp->md[0], &mp->md[1]);
	mp->md[0].sign = sign;

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////

static void define_const(RefNode *m)
{
	RefNode *n;

	n = fs->define_identifier(m, m, "M_PI", NODE_CONST, 0);
	n->u.k.val = fs->float_Value(M_PI);

	n = fs->define_identifier(m, m, "M_E", NODE_CONST, 0);
	n->u.k.val = fs->float_Value(M_E);
}
static void define_func(RefNode *m)
{
	RefNode *n;

	n = fs->define_identifier(m, m, "sin", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, sin, fs->cls_float);

	n = fs->define_identifier(m, m, "cos", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, cos, fs->cls_float);

	n = fs->define_identifier(m, m, "tan", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, tan, fs->cls_float);

	n = fs->define_identifier(m, m, "asin", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, asin, fs->cls_float);

	n = fs->define_identifier(m, m, "acos", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, acos, fs->cls_float);

	n = fs->define_identifier(m, m, "atan", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, atan, fs->cls_float);

	n = fs->define_identifier(m, m, "sinh", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, sinh, fs->cls_float);

	n = fs->define_identifier(m, m, "cosh", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, cosh, fs->cls_float);

	n = fs->define_identifier(m, m, "tanh", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, tanh, fs->cls_float);

	n = fs->define_identifier(m, m, "exp", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, exp, fs->cls_float);

	n = fs->define_identifier(m, m, "log", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, log, fs->cls_float);

	n = fs->define_identifier(m, m, "log10", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, log10, fs->cls_float);

	n = fs->define_identifier(m, m, "sqrt", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func1, 1, 1, sqrt, fs->cls_float);

	n = fs->define_identifier(m, m, "atan2", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_func2, 2, 2, atan2, fs->cls_float, fs->cls_float);

	n = fs->define_identifier(m, m, "abs", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_abs, 1, 1, NULL, fs->cls_number);

	n = fs->define_identifier(m, m, "gcd", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_gcd, 2, 2, NULL, fs->cls_int, fs->cls_int);

	n = fs->define_identifier(m, m, "dot", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_dot, 2, 2, NULL, cls_vector, cls_vector);

	n = fs->define_identifier(m, m, "cross", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_cross, 2, 2, NULL, cls_vector, cls_vector);

	n = fs->define_identifier(m, m, "outer", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_outer, 2, 2, NULL, cls_vector, cls_vector);

	n = fs->define_identifier(m, m, "rand_seed", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_rand_seed, 0, 1, NULL, NULL);

	n = fs->define_identifier(m, m, "rand_bin", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_rand_bin, 1, 1, NULL, fs->cls_int);

	n = fs->define_identifier(m, m, "rand_int", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_rand_int, 1, 1, NULL, fs->cls_int);

	n = fs->define_identifier(m, m, "rand_float", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_rand_float, 0, 0, NULL);

	n = fs->define_identifier(m, m, "rand_gaussian", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_rand_gaussian, 0, 0, NULL);

	n = fs->define_identifier(m, m, "round_down", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_round, 1, 2, (void*) ROUND_DOWN, fs->cls_frac, fs->cls_int);

	n = fs->define_identifier(m, m, "round_up", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_round, 1, 2, (void*) ROUND_UP, fs->cls_frac, fs->cls_int);

	n = fs->define_identifier(m, m, "round_floor", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_round, 1, 2, (void*) ROUND_FLOOR, fs->cls_frac, fs->cls_int);

	n = fs->define_identifier(m, m, "round_ceiling", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_round, 1, 2, (void*) ROUND_CEILING, fs->cls_frac, fs->cls_int);

	n = fs->define_identifier(m, m, "round_half_down", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_round, 1, 2, (void*) ROUND_HALF_DOWN, fs->cls_frac, fs->cls_int);

	n = fs->define_identifier(m, m, "round_half_up", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_round, 1, 2, (void*) ROUND_HALF_UP, fs->cls_frac, fs->cls_int);

	n = fs->define_identifier(m, m, "round_half_even", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, math_round, 1, 2, (void*) ROUND_HALF_EVEN, fs->cls_frac, fs->cls_int);
}
static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

	cls = fs->define_identifier(m, m, "Vector", NODE_CLASS, 0);
	cls_vector = cls;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, vector_new, 0, -1, NULL);

	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, vector_hash, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_dup, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, vector_size, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, vector_empty, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "norm", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, vector_norm, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "to_row_matrix", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_to_matrix, 0, 0, (void*)FALSE);
	n = fs->define_identifier(m, cls, "to_col_matrix", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_to_matrix, 0, 0, (void*)TRUE);

	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_index, 1, 1, NULL, fs->cls_int);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_index_set, 2, 2, NULL, fs->cls_int, fs->cls_number);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_eq, 1, 1, NULL, cls_vector);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_addsub, 1, 1, (void*) FALSE, cls_vector);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_addsub, 1, 1, (void*) TRUE, cls_vector);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_muldiv, 1, 1, (void*) FALSE, fs->cls_float);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, vector_muldiv, 1, 1, (void*) TRUE, fs->cls_float);

	fs->extends_method(cls, fs->cls_obj);


	cls = fs->define_identifier(m, m, "Matrix", NODE_CLASS, 0);
	cls_matrix = cls;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, matrix_new, 0, -1, NULL);
	n = fs->define_identifier(m, cls, "unit", NODE_NEW_N, 0);
	fs->define_native_func_a(n, matrix_i, 1, 1, NULL, fs->cls_int);
	n = fs->define_identifier(m, cls, "zero", NODE_NEW_N, 0);
	fs->define_native_func_a(n, matrix_zero, 2, 2, NULL, fs->cls_int, fs->cls_int);

	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, matrix_hash, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_dup, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, matrix_empty, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "rows", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, matrix_size, 0, 0, (void*)FALSE);
	n = fs->define_identifier(m, cls, "cols", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, matrix_size, 0, 0, (void*)TRUE);

	n = fs->define_identifier(m, cls, "get_row_vector", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_to_vector, 1, 1, (void*)FALSE, fs->cls_int);
	n = fs->define_identifier(m, cls, "get_col_vector", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_to_vector, 1, 1, (void*)TRUE, fs->cls_int);

	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_index, 2, 2, NULL, fs->cls_int, fs->cls_int);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_index_set, 3, 3, NULL, fs->cls_int, fs->cls_int, fs->cls_number);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_eq, 1, 1, NULL, cls_matrix);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_addsub, 1, 1, (void*) FALSE, cls_matrix);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_addsub, 1, 1, (void*) TRUE, cls_matrix);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_multiple, 1, 1, NULL, NULL);
	n = fs->define_identifier(m, cls, "transpose", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_transpose, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "norm", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, matrix_norm, 0, 0, NULL);

	fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_math = m;

	define_class(m);
	define_func(m);
	define_const(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}

