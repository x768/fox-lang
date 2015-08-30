#include "fox_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


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
                RefInt *mp = Value_vp(*v);
                c_buf_len = (mp->bi.size + 1) / 2;
                c_buf = malloc(c_buf_len * 4);
                memcpy(c_buf, mp->bi.d, mp->bi.size * 2);
                if ((mp->bi.size % 2) != 0) {
                    memset(c_buf + mp->bi.size * 2, 0, 1);
                }
            } else {
                uint32_t ival = Value_integral(v1);
                c_buf_len = 1;
                c_buf = malloc(c_buf_len * 4);
                memcpy(c_buf, &ival, 4);
            }
        } else if (v1_type == fs->cls_str || v1_type == fs->cls_bytes) {
            RefStr *s = Value_vp(v1);
            c_buf_len = (s->size + 3) / 4;
            c_buf = malloc(c_buf_len * 4);
            memcpy(c_buf, s->c, s->size);
            if ((s->size % 4) != 0) {
                memset(c_buf + s->size, 0, c_buf_len * 4 - s->size);
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
static int math_rand_bytes(Value *vret, Value *v, RefNode *node)
{
    int n, i;
    RefStr *rs;
    Value v1 = v[1];
    int64_t size;
    int err = FALSE;

    if (!rand_initialized) {
        init_rand48();
    }

    size = fs->Value_int64(v1, &err);
    if (size < 0) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Must be >= 0 (argument #1)");
        return FALSE;
    }
    if (err || size > fs->max_alloc) {
        fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    n = (size + 3) / 4;

    rs = fs->refstr_new_n(fs->cls_bytes, n * 4);
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
        BigInt mp;
        int i, n;
        RefInt *mpval = Value_vp(v1);
        RefInt *rem;

        if (mpval->bi.sign <= 0) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Must be > 0 (argument #1)");
            return FALSE;
        }
        n = (mpval->bi.size + 1) / 2;

        fs->BigInt_init(&mp);
        for (i = 0; i < n; i++) {
            uint32_t r = gen_int32();

            fs->BigInt_lsh(&mp, 16);
            fs->BigInt_add_d(&mp, r & 0xFFFF);
            fs->BigInt_lsh(&mp, 16);
            fs->BigInt_add_d(&mp, r >> 16);
        }
        rem = fs->buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(rem);
        fs->BigInt_init(&rem->bi);
        fs->BigInt_divmod(NULL, &rem->bi, &mp, &mpval->bi);
        fs->BigInt_close(&mp);

        fs->fix_bigint(vret, &rem->bi);
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
    *vret = fs->float_Value(fs->cls_float, dval);

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
    *vret = fs->float_Value(fs->cls_float, dval);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void define_rand_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "rand_seed", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_rand_seed, 0, 1, NULL, NULL);

    n = fs->define_identifier(m, m, "rand_bytes", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_rand_bytes, 1, 1, NULL, fs->cls_int);

    n = fs->define_identifier(m, m, "rand_int", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_rand_int, 1, 1, NULL, fs->cls_int);

    n = fs->define_identifier(m, m, "rand_float", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_rand_float, 0, 0, NULL);

    n = fs->define_identifier(m, m, "rand_gaussian", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_rand_gaussian, 0, 0, NULL);
}
