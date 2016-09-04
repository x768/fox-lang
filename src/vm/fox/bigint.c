#include "bigint.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>


enum {
    SCALE_MAX = 32767,
    SCALE_MIN = -32767,
};

// 1 / log2(n)
static const double s_log_r_2[] = {
    1.000000000, 0.630929754, 0.500000000, 0.430676558,  //  2  3  4  5
    0.386852807, 0.356207187, 0.333333333, 0.315464877,  //  6  7  8  9
    0.301029996, 0.289064826, 0.278942946, 0.270238154,  // 10 11 12 13
    0.262649535, 0.255958025, 0.250000000, 0.244650542,  // 14 15 16 17
    0.239812467, 0.235408913, 0.231378213, 0.227670249,  // 18 19 20 21
    0.224243824, 0.221064729, 0.218104292, 0.215338279,  // 22 23 24 25
    0.212746054, 0.210309918, 0.208014598, 0.205846832,  // 26 27 28 29
    0.203795047, 0.201849087, 0.200000000, 0.198239863,  // 30 31 32 33
    0.196561632, 0.194959022, 0.193426404,               // 34 35 36
};

static int max_alloc = 32768;

/////////////////////////////////////////////////////////////////////////////////

/**
 * alloc_sizeを増やす
 * sizeはそのまま
 */
int BigInt_reserve(BigInt *bi, int size)
{
    if (bi->alloc_size < size) {
        int alloc_size = bi->alloc_size;
        if (alloc_size == 0) {
            alloc_size = 16;
        }
        while (alloc_size < size) {
            alloc_size *= 2;
        }
        if (alloc_size > max_alloc) {
            return 0;
        }
        bi->alloc_size = alloc_size;
        bi->d = realloc(bi->d, sizeof(uint16_t) * alloc_size);
        if (alloc_size > bi->size) {
            memset(bi->d + bi->size, 0, (alloc_size - bi->size) * sizeof(uint16_t));
        }
    }
    return 1;
}
/**
 * dの上位が0の場合、sizeを切り詰める
 */
static void BigInt_shrink(BigInt *bi)
{
    if (bi->size > 0) {
        int i = bi->size - 1;
        if (bi->d[i] == 0) {
            for (; i > 0; i--) {
                if (bi->d[i - 1] != 0) {
                    break;
                }
            }
            bi->size = i;
            if (i == 0) {
                bi->sign = 0;
                bi->alloc_size = 0;
                free(bi->d);
                bi->d = NULL;
            }
        }
    }
}

static void int16_BigInt(BigInt *bi, int val)
{
    if (val > 0) {
        BigInt_reserve(bi, 1);
        bi->d[0] = (uint16_t)val;
        bi->size = 1;
        bi->sign = 1;
    } else if (val < 0) {
        BigInt_reserve(bi, 1);
        bi->d[0] = (uint16_t)-val;
        bi->size = 1;
        bi->sign = -1;
    } else {
        bi->size = 0;
        bi->sign = 0;
    }
}
static int BigInt_lsh_d(BigInt *bi, int n)
{
    if (!BigInt_reserve(bi, bi->size + n)) {
        return 0;
    }
    memmove(bi->d + n, bi->d, bi->size * sizeof(uint16_t));
    memset(bi->d, 0, n * sizeof(uint16_t));
    bi->size += n;
    return 1;
}

/**
 * bi *= m
 * m: 0..32767
 */
int BigInt_mul_sd(BigInt *bi, int m)
{
    int i;

    if (m == 0) {
        bi->sign = 0;
        bi->size = 0;
        return 1;
    }

    // e.g.
    // bi * 4 => bi << 2
    for (i = 0; i < BIGINT_DIGIT_BITS; i++) {
        if (m == (1 << i)) {
            return BigInt_lsh(bi, i);
        }
    }

    {
        uint32_t carry = 0;
        uint16_t *dst = bi->d;

        for (i = 0; i < bi->size; i++) {
            uint32_t d = *dst * m + carry;
            *dst = (uint16_t)d;
            dst++;
            carry = (d >> BIGINT_DIGIT_BITS) & BIGINT_MASK_BITS;
        }
        if (carry != 0) {
            if (!BigInt_reserve(bi, bi->size + 1)) {
                return 0;
            }
            bi->d[bi->size] = carry;
            bi->size++;
        }
    }
    return 1;
}
/**
 * bi /= dv
 * mod = bi % dv
 * 
 * dv: 1..32767
 */
void BigInt_divmod_sd(BigInt *bi, int dv, uint16_t *mod)
{
    int i;

    if (dv <= 0) {
        // Error
        return;
    }
    if (bi->size == 0) {
        *mod = 0;
        return;
    }
    // e.g.
    // bi / 4 => bi >> 2
    for (i = 0; i < BIGINT_DIGIT_BITS; i++) {
        if (dv == (1 << i)) {
            *mod = bi->d[0] & ((1 << i) - 1);
            BigInt_rsh(bi, i);
            return;
        }
    }

    {
        int first = 1;
        uint32_t carry = 0;
        uint16_t *dst = bi->d + bi->size - 1;
        int size = bi->size;

        for (i = 0; i < size; i++) {
            int d = *dst | (carry << BIGINT_DIGIT_BITS);
            if (d >= dv) {
                *dst = d / dv;
                carry = d % dv;
                first = 0;
            } else {
                *dst = 0;
                carry = d;
                if (first) {
                    bi->size--;
                }
            }
            dst--;
        }
        if (bi->size == 0) {
            bi->sign = 0;
        }
        *mod = carry;
    }
}
/**
 * absolute values
 * a += b
 * b: 0 ... 32767
 */
static int BigInt_add_sd(BigInt *a, int b)
{
    int i;
    uint32_t carry = b;

    for (i = 0; i < a->size; i++) {
        if (carry > 0) {
            uint32_t d = a->d[i] + carry;
            a->d[i] = (uint16_t)d;
            carry = d >> BIGINT_DIGIT_BITS;
        } else {
            break;
        }
    }
    if (carry != 0) {
        if (!BigInt_reserve(a, a->size + 1)) {
            return 0;
        }
        a->d[a->size] = carry;
        a->size++;
    }
    return 1;
}
/**
 * absolute values
 * a -= b
 * b: 0 ... 32767
 */
static void BigInt_sub_sd(BigInt *a, int b)
{
    int i;
    uint32_t borrow = b;

    for (i = 0; i < a->size; i++) {
        a->d[i] -= borrow;
        if (a->d[i] < borrow) {
            break;
        } else {
            borrow = 1;
        }
    }
    BigInt_shrink(a);
}
/**
 * absolute values
 * a += b
 */
static int BigInt_add_s(BigInt *a, const BigInt *b)
{
    int i;
    uint32_t carry = 0;

    if (a->alloc_size < b->size) {
        if (!BigInt_reserve(a, b->size)) {
            return 0;
        }
    }
    if (a->size < b->size) {
        memset(a->d + a->size, 0, (b->size - a->size) * sizeof(uint16_t));
        a->size = b->size;
    }
    for (i = 0; i < a->size; i++) {
        uint32_t d = a->d[i] + b->d[i] + carry;
        a->d[i] = (uint16_t)d;
        carry = d >> BIGINT_DIGIT_BITS;
    }
    if (carry != 0) {
        if (!BigInt_reserve(a, a->size + 1)) {
            return 0;
        }
        a->d[a->size] = carry;
        a->size++;
    }
    return 1;
}
/**
 * a += b
 */
int BigInt_add_d(BigInt *a, int b)
{
    if (b == 0) {
        return 1;
    }
    if (a->sign == 0) {
        int16_BigInt(a, b);
        return 1;
    }
    if ((a->sign > 0 && b > 0) || (a->sign < 0 && b < 0)) {
        if (b < 0) {
            b = -b;
        }
        return BigInt_add_sd(a, b);
    }
    // if |a| >= |b|
    if (b < 0) {
        b = -b;
    }
    if (a->size > 1 || a->d[0] > b) {
        BigInt_sub_sd(a, b);
    } else {
        a->d[0] -= b;
        if (a->d[0] == 0) {
            a->size = 0;
            a->sign = 0;
        } else {
            a->sign = -a->sign;
        }
    }
    return 1;
}
/**
 * absolute values
 * rev = 0, a = a - b  [a >= b]
 * rev = 1, a = b - a  [a <= b]
 */
static int BigInt_sub_s(BigInt *a, const BigInt *b, int rev)
{
    int i;
    int borrow = 0;

    if (rev == 0) {
        for (i = 0; i < a->size; i++) {
            int32_t d = a->d[i] - b->d[i] - borrow;
            if (d < 0) {
                d += 1 << BIGINT_DIGIT_BITS;
                borrow = 1;
            } else {
                borrow = 0;
            }
            a->d[i] = d;
        }
    } else {
        if (a->alloc_size < b->size) {
            if (!BigInt_reserve(a, b->size)) {
                return 0;
            }
        }
        if (a->size < b->size) {
            memset(a->d + a->size, 0, (b->size - a->size) * sizeof(uint16_t));
        }
        for (i = 0; i < a->size; i++) {
            int32_t d = b->d[i] - a->d[i] - borrow;
            if (d < 0) {
                d += 1 << BIGINT_DIGIT_BITS;
                borrow = 1;
            } else {
                borrow = 0;
            }
            a->d[i] = d;
        }
    }
    BigInt_shrink(a);
    return 1;
}
/**
 * |a| > |b|  ... 1, 2, ..
 * |a| < |b|  ... -1, -2, ..
 * |a| == |b| ... 0
 */
static int BigInt_cmp_abs(const BigInt *a, const BigInt *b)
{
    int i;
    if (a->size != b->size) {
        return a->size - b->size;
    }
    for (i = a->size - 1; i >= 0; i--) {
        int cmp = a->d[i] - b->d[i];
        if (cmp != 0) {
            return cmp;
        }
    }
    return 0;
}
static int BigInt_divmod_normalize(BigInt *a, BigInt *b)
{
    uint16_t d = 0;
    uint16_t t = b->d[b->size - 1];

    while ((t & 0x8000) == 0) {
        t <<= 1;
        d++;
    }
    if (d > 0) {
        BigInt_lsh(a, d);
        BigInt_lsh(b, d);
    }
    return d;
}
/**
 * a = a / b
 * b = a % b
 * a >= 0
 * b >= 0
 */
static int BigInt_divmod_s(BigInt *a, BigInt *b)
{
    BigInt quot, rem, tmp;
    int d, i;

    if (b->sign == 0) {
        // Error
        return 0;
    }
    d = BigInt_divmod_normalize(a, b);

    BigInt_init(&quot);
    BigInt_init(&rem);
    BigInt_init(&tmp);
    BigInt_reserve(&quot, a->size);
    BigInt_reserve(&rem, a->size);
    BigInt_reserve(&tmp, a->size);

    i = a->size - 1;
    while (i >= 0) {
        uint32_t q;
        while (BigInt_cmp_abs(&rem, b) < 0 && i >= 0) {
            if (!BigInt_lsh_d(&rem, 1)) {
                goto ERROR_END;
            }
            if (!BigInt_lsh_d(&quot, 1)) {
                goto ERROR_END;
            }
            rem.d[0] = a->d[i];
            if (rem.d[0] != 0) {
                rem.sign = 1;
            }
            i--;
        }
        if (BigInt_cmp_abs(&rem, b) < 0) {
            break;
        }
        q = rem.d[rem.size - 1];
        if (q <= b->d[b->size - 1] && rem.size > 1) {
            q = (q << BIGINT_DIGIT_BITS) | rem.d[rem.size - 2];
        }
        q /= b->d[b->size - 1];

        if (q > UINT16_MAX) {
            q = UINT16_MAX;
        }
        BigInt_copy(&tmp, b);
        if (!BigInt_mul_sd(&tmp, q)) {
            goto ERROR_END;
        }
        while (BigInt_cmp_abs(&tmp, &rem) > 0) {
            q--;
            BigInt_sub_s(&tmp, b, 0);
        }
        BigInt_sub_s(&rem, &tmp, 0);
        quot.d[0] = q;
        if (q != 0) {
            quot.sign = 1;
        }
    }
    if (d > 0) {
        BigInt_rsh(&rem, d);
    }
    BigInt_shrink(&quot);
    BigInt_shrink(&rem);

    BigInt_close(a);
    BigInt_close(b);
    *a = quot;
    *b = rem;

    return 1;
ERROR_END:
    BigInt_close(&quot);
    BigInt_close(&rem);
    BigInt_close(&tmp);
    return 0;
}

// m = (m * 10) % d
static void mul10mod(BigInt *m, BigInt *d)
{
    BigInt_mul_sd(m, 10);
    BigInt_divmod(0, m, m, d);
}
int BigInt_get_recurrence(BigInt *mp, int *ret, int limit)
{
    int begin = 0;
    int end = 0;
    BigInt m;
    BigInt p;

    if (mp->sign == 0) {
        ret[0] = 0;
        ret[1] = 0;
        return FALSE;
    }
    BigInt_init(&m);
    int64_BigInt(&m, 1);
    BigInt_init(&p);
    int64_BigInt(&p, 1);

    for (;;) {
        if (end >= limit) {
            begin = end;
            goto RECR;
        }
        end++;
        // m = (m * 10) % mp;
        mul10mod(&m, mp);

        // p = (p * 10) % mp;
        // p = (p * 10) % mp;
        mul10mod(&p, mp);
        mul10mod(&p, mp);
        if (BigInt_cmp(&m, &p) == 0) {
            break;
        }
    }

    if (p.sign == 0 && m.sign == 0) {
        ret[0] = end;
        ret[1] = end;
        goto NOT_RECR;
    }

    int64_BigInt(&p, 1);
    begin = 1;
    while (BigInt_cmp(&m, &p) != 0) {
        if (begin >= limit) {
            end = begin;
            goto RECR;
        }
        begin++;
        // m = (m * 10) % mp;
        mul10mod(&m, mp);

        // p = (p * 10) % mp;
        mul10mod(&p, mp);
    }

    //p = (p * 10) % n;
    mul10mod(&p, mp);
    end = begin;
    while (BigInt_cmp(&m, &p) != 0) {
        end++;
        //p = (p * 10) % mp;
        mul10mod(&p, mp);
    }
RECR:
    ret[0] = begin;
    ret[1] = end;
    BigInt_close(&m);
    BigInt_close(&p);
    return TRUE;

NOT_RECR:
    BigInt_close(&m);
    BigInt_close(&p);
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////

void BigInt_set_max_alloc(int max)
{
    if (max > 32768) {
        max_alloc = max_alloc / sizeof(uint16_t);
    }
}

void BigInt_init(BigInt *bi)
{
    bi->sign = 0;
    bi->size = 0;
    bi->alloc_size = 0;
    bi->d = NULL;
}

/**
 * bi = (bigint)value
 */
void int64_BigInt(BigInt *bi, int64_t value)
{
    if (value == INT64_MIN) {
        BigInt_reserve(bi, 5);
        bi->size = 5;
        bi->sign = -1;
        memset(bi->d, 0, 4 * sizeof(uint16_t));
        bi->d[4] = 1;
    } else if (value < 0) {
        uint64_BigInt(bi, -value);
        bi->sign = -1;
    } else if (value > 0) {
        uint64_BigInt(bi, value);
    }
}
/**
 * bi = (bigint)value
 */
void uint64_BigInt(BigInt *bi, uint64_t value)
{
    if (value == 0) {
        bi->sign = 0;
        bi->size = 0;
        bi->alloc_size = 0;
        bi->d = NULL;
    } else {
        int i = 0;
        BigInt_reserve(bi, 4);

        while (value != 0) {
            bi->d[i++] = (uint16_t)value;
            value >>= BIGINT_DIGIT_BITS;
        }
        bi->size = i;
        bi->sign = 1;
    }
}
/**
 * bi = (bigint)value
 */
void double_BigInt(BigInt *bi, double value)
{
    enum {
        EXP_OFFSET = 53,
    };
    int ex;
    union {
        double v;
        uint64_t ival;
    } u;
    u.v = (value < 0.0 ? -value : value);

    // IEEE754決め打ち
    ex = ((u.ival >> 52) & 0x7ff) - 1022;
    u.ival &=  0xFffffFFFFffffLL;
    u.ival |= 0x10000000000000LL;

	uint64_BigInt(bi, u.ival);
    if (ex < EXP_OFFSET) {
        BigInt_rsh(bi, EXP_OFFSET - ex);
    } else if (ex > EXP_OFFSET) {
        BigInt_lsh(bi, ex - EXP_OFFSET);
    }

    if (bi->size > 0) {
		if (value < 0.0) {
			bi->sign = -1;
		} else {
			bi->sign = 1;
		}
	} else {
		bi->sign = 0;
	}
}
/**
 * radix: 2 - 36
 * size: strlen(size) or -1
 */
int cstr_BigInt(BigInt *bi, int radix, const char *str, int size)
{
    int i;

    if (radix < 2 || radix > 36) {
        return 0;
    }
    if (size < 0) {
        size = strlen(str);
    }
    if (size >= 1 && str[0] == '-') {
        bi->sign = -1;
        str++;
        size--;
    } else {
        bi->sign = 1;
    }

    {
        int reserve_bits = (int)((double)size / s_log_r_2[radix - 2]);
        if (!BigInt_reserve(bi, reserve_bits / BIGINT_DIGIT_BITS + 1)) {
            return 0;
        }
        bi->size = 0;
    }

    for (i = 0; i < size; i++) {
        int ch = str[i];
        if (ch >= '0' && ch <= '9') {
            ch = ch - '0';
        } else if (ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 10;
        } else if (ch >= 'a' && ch <= 'z') {
            ch = ch - 'a' + 10;
        } else {
            continue;
        }
        if (!BigInt_mul_sd(bi, radix)) {
            return 0;
        }
        if (!BigInt_add_sd(bi, ch)) {
            return 0;
        }
    }
    if (bi->size == 0) {
        bi->sign = 0;
    }

    return 1;
}
void BigInt_copy(BigInt *dst, const BigInt *src)
{
    if (src->sign == 0) {
        dst->size = 0;
        dst->sign = 0;
    } else {
        BigInt_reserve(dst, src->size);
        dst->size = src->size;
        dst->sign = src->sign;
        memcpy(dst->d, src->d, dst->size * sizeof(uint16_t));
    }
}
void BigInt_close(BigInt *bi)
{
    free(bi->d);
}

/////////////////////////////////////////////////////////////////////////////////////

/**
 * value = (int32_t)bi
 * -INT32_MAX ... INT32_MAX
 * otherwise => return INT32_MIN
 */
int32_t BigInt_int32(const BigInt *bi)
{
    int32_t value = 0;

    switch (bi->size) {
    case 0:
        break;
    case 1:
        value = bi->d[0];
        break;
    case 2:
        if ((bi->d[1] & 0x8000) != 0) {
            return INT32_MIN;
        }
        value = bi->d[0] | (bi->d[1] << BIGINT_DIGIT_BITS);
        break;
    default:
        return INT32_MIN;
    }
    if (bi->sign < 0) {
        return -value;
    } else {
        return value;
    }
}
/**
 * value = (int64_t)bi
 * INT64_MIN ... INT64_MAX => return 1
 */
int BigInt_int64(const BigInt *bi, int64_t *value)
{
    int i;
    int64_t v = 0LL;

    switch (bi->size) {
    case 0:
        break;
    case 4:
        if (bi->sign < 0 && bi->d[0] == 0 && bi->d[1] == 0 && bi->d[2] == 0 && bi->d[3] == 0x8000) {
            *value = INT64_MIN;
            return 1;
        }
        if ((bi->d[3] & 0x8000) != 0) {
            *value = bi->sign < 0 ? INT64_MIN : INT64_MAX;
            return 0;
        }
        // fall through
    case 1:
    case 2:
    case 3:
        for (i = 0; i < bi->size; i++) {
            v = (v << BIGINT_DIGIT_BITS) | bi->d[i];
        }
        break;
    default:
        *value = bi->sign < 0 ? INT64_MIN : INT64_MAX;
        return 0;
    }
    if (bi->sign < 0) {
        *value = -v;
    } else {
        *value = v;
    }
    return 1;
}
/**
 * value = (double)bi
 */
double BigInt_double(const BigInt *bi)
{
    int size = bi->size;
    double result = 0.0;
    uint64_t ival = 0LL;
    int ex = size * BIGINT_DIGIT_BITS;
    int i;

    // 上位53ビットを取り出す
    for (i = 0; i < 4; i++) {
        ival <<= BIGINT_DIGIT_BITS;
        if (i < size) {
            ival |= bi->d[size - i - 1];
        }
    }
    if (ival == 0ULL) {
        return 0.0;
    }

    // 52bit目が立つように調整する
    if ((ival & 0xFFF0000000000000ULL) == 0ULL) {
        int bits = 0;
        while ((ival & 0xFFF0000000000000ULL) == 0ULL) {
            ival <<= 1;
            bits++;
        }
        // 下に隠れたビットを補う
        if (size - 5 >= 0) {
            uint16_t d = bi->d[size - 5];
            d >>= BIGINT_DIGIT_BITS - bits;
            ival |= d;
        }
        ex -= bits;
    } else {
        while ((ival & 0xFFE0000000000000ULL) != 0ULL) {
            ival >>= 1;
            ex++;
        }
    }

    // 指数
    ex += 1011;
    if (ex > 0x7ff) {
        result = 1.0 / 0.0;  // infinity
    } else {
        ival &= 0xFffffFFFFffffLL;
        ival |= ((uint64_t)ex) << 52;
        memcpy(&result, &ival, 8);
    }

    if (bi->sign < 0) {
        result = -result;
    }
    return result;
}

int BigInt_str_bufsize(const BigInt *bi, int radix)
{
    int size = 1;

    if (bi->sign <= 0) {
        size++;
    }
    if (radix >= 2 && radix <= 36) {
        int bits = bi->size * BIGINT_DIGIT_BITS;
        size += (int)((double)bits * s_log_r_2[radix - 2]) + 1;
    }

    return size + 1;
}
int BigInt_str(const BigInt *bi, int radix, char *str, int upper)
{
    char *dst = str;

    if (bi->sign == 0) {
        *dst++ = '0';
        *dst = '\0';
        return dst - str;
    }
    if (bi->sign < 0) {
        *dst++ = '-';
    }
    if (radix >= 2 && radix <= 36) {
        BigInt b;
        char *p1 = dst;
        char *p2;
        BigInt_init(&b);
        BigInt_copy(&b, bi);

        while (b.sign != 0) {
            uint16_t mod;
            BigInt_divmod_sd(&b, radix, &mod);
            if (mod < 10) {
                *dst++ = '0' + mod;
            } else if (upper) {
                *dst++ = 'A' - 10 + mod;
            } else {
                *dst++ = 'a' - 10 + mod;
            }
        }
        p2 = dst - 1;
        // reverse [p1 .. p2]
        while (p1 < p2) {
            int tmp = *p1;
            *p1 = *p2;
            *p2 = tmp;
            p1++;
            p2--;
        }
    }

    *dst = '\0';
    return dst - str;
}

/////////////////////////////////////////////////////////////////////////////////////

uint32_t BigInt_hash(const BigInt *bi)
{
    int i;
    uint32_t h = 0;

    for (i = 0; i < bi->size; i++) {
        h = h * 31 + bi->d[i];
    }
    return h;
}
/**
 * a < b  ... return < 0
 * a > b  ... return > 0
 * a == b ... return 0
 */
int BigInt_cmp(const BigInt *a, const BigInt *b)
{
    if (a->sign != b->sign) {
        return a->sign - b->sign;
    } else {
        int cmp = BigInt_cmp_abs(a, b);
        if (a->sign < 0) {
            cmp = -cmp;
        }
        return cmp;
    }
}

/////////////////////////////////////////////////////////////////////////////////////

/**
 * bi <<= sh
 */
int BigInt_lsh(BigInt *bi, uint32_t sh)
{
    int shift_d = sh / BIGINT_DIGIT_BITS;
    int shift_b = sh % BIGINT_DIGIT_BITS;

    if (!BigInt_reserve(bi, bi->size + shift_d + 1)) {
        return 0;
    }
    if (shift_d > 0) {
        memmove(bi->d + shift_d, bi->d, bi->size * sizeof(uint16_t));
        memset(bi->d, 0, shift_d * sizeof(uint16_t));
    }

    if (shift_b > 0) {
        uint32_t carry = 0;
        uint16_t *dst = bi->d + shift_d;
        int i;
        for (i = 0; i < bi->size; i++) {
            uint32_t d = *dst << shift_b;
            *dst = carry | (d & BIGINT_MASK_BITS);
            dst++;
            carry = d >> BIGINT_DIGIT_BITS;
        }
        if (carry != 0) {
            *dst = carry;
            bi->size += shift_d + 1;
        } else {
            bi->size += shift_d;
        }
    } else {
        bi->size += shift_d;
    }

    return 1;
}
/**
 * bi >>= sh
 */
void BigInt_rsh(BigInt *bi, uint32_t sh)
{
    int shift_d = sh / BIGINT_DIGIT_BITS;
    int shift_b = sh % BIGINT_DIGIT_BITS;

    if (shift_d > 0) {
        bi->size -= shift_d;
        memmove(bi->d, bi->d + shift_d, bi->size * sizeof(uint16_t));
    }
    if (shift_b > 0) {
        uint32_t carry = 0;
        uint16_t *dst = bi->d + bi->size - 1;
        int i;
        for (i = 0; i < bi->size; i++) {
            uint32_t d = *dst;
            *dst = carry | (d >> shift_b);
            dst--;
            carry = d << (BIGINT_DIGIT_BITS - shift_b);
        }
        BigInt_shrink(bi);
    }
}
void BigInt_and(BigInt *a, const BigInt *b)
{
    int i;

    if (a->size < b->size) {
        BigInt_reserve(a, b->size);
        a->size = b->size;
    }
    for (i = 0; i < a->size; i++) {
        a->d[i] &= b->d[i];
    }
    BigInt_shrink(a);
}
void BigInt_or(BigInt *a, const BigInt *b)
{
    int i;

    if (a->size < b->size) {
        BigInt_reserve(a, b->size);
        a->size = b->size;
    }
    for (i = 0; i < a->size; i++) {
        a->d[i] |= b->d[i];
    }
}
void BigInt_xor(BigInt *a, const BigInt *b)
{
    int i;

    if (a->size < b->size) {
        BigInt_reserve(a, b->size);
        a->size = b->size;
    }
    for (i = 0; i < a->size; i++) {
        a->d[i] ^= b->d[i];
    }
    BigInt_shrink(a);
}

/////////////////////////////////////////////////////////////////////////////////////////

/**
 * a += b
 */
int BigInt_add(BigInt *a, const BigInt *b)
{
    if (b->sign == 0) {
        // do nothing
        return 1;
    }
    if (a->sign == 0) {
        BigInt_copy(a, b);
        return 1;
    }
    if (a->sign == b->sign) {
        BigInt_add_s(a, b);
    } else if (BigInt_cmp_abs(a, b) > 0) {
        BigInt_sub_s(a, b, 0);
    } else {
        BigInt_sub_s(a, b, 1);
        a->sign = -a->sign;
    }
    return 1;
}
/**
 * a -= b
 */
int BigInt_sub(BigInt *a, const BigInt *b)
{
    if (b->sign == 0) {
        // do nothing
        return 1;
    }
    if (a->sign == 0) {
        BigInt_copy(a, b);
        a->sign = -a->sign;
        return 1;
    }
    if (a->sign != b->sign) {
        BigInt_add_s(a, b);
    } else if (BigInt_cmp_abs(a, b) > 0) {
        BigInt_sub_s(a, b, 0);
    } else {
        BigInt_sub_s(a, b, 1);
        a->sign = -a->sign;
    }
    return 1;
}
/**
 * ret = a * b
 * ret == aの場合、一時バッファを使用する
 */
int BigInt_mul(BigInt *ret, const BigInt *a, const BigInt *b)
{
    int i, j;
    BigInt ret_buf;
    BigInt *ret_tmp = NULL;

    if (a->sign == 0 || b->sign == 0) {
        ret->sign = 0;
        ret->size = 0;
        return 1;
    }

    if (ret == a) {
        BigInt_init(&ret_buf);
        ret_tmp = ret;
        ret = &ret_buf;
    }
    ret->sign = a->sign * b->sign;
    if (!BigInt_reserve(ret, a->size + b->size + 1)) {
        if (ret_tmp != NULL) {
            BigInt_close(&ret_buf);
        }
        return 0;
    }
    ret->size = a->size + b->size + 1;
    memset(ret->d, 0, ret->size * sizeof(uint16_t));

    for (i = 0; i < b->size; i++) {
        uint16_t carry = 0;
        for (j = 0; j < a->size; j++) {
            uint32_t d = a->d[j] * b->d[i] + ret->d[i + j] + carry;
            ret->d[i + j] = (uint16_t)d;
            carry = d >> BIGINT_DIGIT_BITS;
        }
        if (carry != 0) {
            ret->d[i + j] += carry;
        }
    }
    BigInt_shrink(ret);

    if (ret_tmp != NULL) {
        BigInt_close(ret_tmp);
        *ret_tmp = ret_buf;
    }
    return 1;
}
/**
 * ret = a / b
 * mod = a % b
 */
int BigInt_divmod(BigInt *ret, BigInt *mod, const BigInt *a, const BigInt *b)
{
    BigInt ret_buf;
    BigInt *ret_tmp = NULL;
    BigInt mod_buf;
    BigInt *mod_tmp = NULL;

    if (b->sign == 0) {
        // Error
        return 0;
    }
    if (a->sign == 0) {
        if (ret != NULL) {
            ret->sign = 0;
            ret->size = 0;
        }
        if (mod != NULL) {
            mod->sign = 0;
            mod->size = 0;
        }
        return 1;
    }
    if (ret == a || ret == b || ret == NULL) {
        BigInt_init(&ret_buf);
        ret_tmp = ret;
        ret = &ret_buf;
    }
    if (mod == a || mod == b || mod == NULL) {
        BigInt_init(&mod_buf);
        mod_tmp = mod;
        mod = &mod_buf;
    }
    BigInt_copy(ret, a);
    BigInt_copy(mod, b);

    if (!BigInt_divmod_s(ret, mod)) {
        if (ret_tmp != NULL) {
            BigInt_close(&ret_buf);
        }
        if (mod_tmp != NULL) {
            BigInt_close(&mod_buf);
        }
        return 0;
    }
    // C99
    if (ret->size > 0) {
        ret->sign = a->sign * b->sign;
    }
    if (mod->size > 0) {
        mod->sign = a->sign;
    }
    if (ret_tmp != NULL) {
        BigInt_close(ret_tmp);
        *ret_tmp = ret_buf;
    }
    if (mod_tmp != NULL) {
        BigInt_close(mod_tmp);
        *mod_tmp = mod_buf;
    }
    return 1;
}
/**
 * a = a ** n
 */
int BigInt_pow(BigInt *a, uint32_t n)
{
    int i;
    BigInt bi;

    if (n == 0) {
        int16_BigInt(a, 1);
        return 1;
    }
    if (n == 1) {
        return 1;
    }
    if (a->sign == 0) {
        return 1;
    }
    if (a->sign < 0) {
        a->sign = (n & 1) != 0 ? -1 : 1;
    }

    BigInt_init(&bi);
    int16_BigInt(&bi, 1);
    for (i = 0; i < 32; i++) {
        if ((n & (1U << i)) != 0) {
            // bi = bi * a
            if (a->size == 1) {
                if (!BigInt_mul_sd(&bi, a->d[0])) {
                    BigInt_close(&bi);
                    return 0;
                }
            } else {
                BigInt tmp;
                BigInt_init(&tmp);
                if (!BigInt_mul(&tmp, &bi, a)) {
                    BigInt_close(&bi);
                    BigInt_close(&tmp);
                    return 0;
                }
                BigInt_close(&bi);
                bi = tmp;
            }
        }
        if (n <= (1U << i)) {
            break;
        }
        // a = a * a
        if (a->size == 1) {
            if (!BigInt_mul_sd(a, a->d[0])) {
                BigInt_close(&bi);
                return 0;
            }
        } else {
            BigInt tmp;
            BigInt_init(&tmp);
            if (!BigInt_mul(&tmp, a, a)) {
                BigInt_close(&bi);
                BigInt_close(&tmp);
                return 0;
            }
            BigInt_close(a);
            *a = tmp;
        }
    }
    BigInt_close(a);
    *a = bi;

    return 1;
}
/**
 * Ignore sign
 */
int BigInt_gcd(BigInt *ret, const BigInt *a, const BigInt *b)
{
    BigInt u, v, t;
    int n2 = 0;

    if (a->sign == 0) {
        BigInt_copy(ret, b);
        return 1;
    }
    if (b->sign == 0) {
        BigInt_copy(ret, a);
        return 1;
    }
    BigInt_init(&u);
    BigInt_init(&v);
    BigInt_init(&t);
    BigInt_copy(&u, a);
    BigInt_copy(&v, b);

    u.sign = 1;
    v.sign = 1;

    // Divide out common factors of 2 until at least 1 of a, b is even
    while (u.sign > 0 && (u.d[0] & 1) == 0 && v.sign > 0 && (v.d[0] & 1) == 0) {
        BigInt_rsh(&u, 1);
        BigInt_rsh(&v, 1);
        n2++;
    }
    // Initialize t
    if (u.sign > 0 && (u.d[0] & 1) != 0) {
        BigInt_copy(&t, &v);
        t.sign = -v.sign;
    } else {
        BigInt_copy(&t, &u);
    }

    for(;;) {
        while (t.sign > 0 && (t.d[0] & 1) == 0) {
            BigInt_rsh(&t, 1);
        }
        if (t.sign > 0) {
            BigInt_copy(&u, &t);
        } else {
            BigInt_copy(&v, &t);
            v.sign = -t.sign;
        }
        BigInt_copy(&t, &u);
        BigInt_sub(&t, &v);
        if (t.sign == 0) {
            break;
        }
    }

    BigInt_copy(ret, &u);
    if (n2 > 0) {
        BigInt_lsh(ret, n2);
    }
    BigInt_close(&u);
    BigInt_close(&v);
    BigInt_close(&t);

    return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////

// 有理数を通分
void BigRat_fix(BigInt *rat)
{
    BigInt gcd;

    BigInt_init(&gcd);
    BigInt_gcd(&gcd, &rat[0], &rat[1]);

    // 1より大きい場合のみ、最大公約数で割る
    if (gcd.size > 1 || gcd.d[0] > 1) {
        BigInt tmp;
        BigInt_init(&tmp);

        BigInt_copy(&tmp, &gcd);
        BigInt_divmod_s(&rat[0], &tmp);
        BigInt_copy(&tmp, &gcd);
        BigInt_divmod_s(&rat[1], &tmp);

        BigInt_close(&tmp);
    }

    // 符号はrat[0]だけに付ける
    if (rat[1].sign < 0) {
        rat[1].sign = 1;
        rat[0].sign *= -1;
    }
}
int cstr_BigRat(BigInt *rat, const char *src, const char **end)
{
    const char *p = src;
    int scale = 0;
    int sign = 1;

    // 数値以外の文字が出現するまで進める
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        sign = -1;
        p++;
    }
    while (isdigit(*p & 0xFF)) {
        p++;
    }

    // 実数部
    if (*p == '.') {
        char *c_buf = malloc(strlen(src) + 1);
        char *c_dst = c_buf + (p - src);
        memcpy(c_buf, src, p - src);

        p++;
        while (isdigit(*p)) {
            *c_dst++ = *p;
            scale--;
            p++;
        }
        *c_dst = '\0';
        cstr_BigInt(&rat[0], 10, c_buf, -1);
        free(c_buf);
    } else {
        cstr_BigInt(&rat[0], 10, src, p - src);
    }

    // 指数部(存在する場合)
    if (*p == 'e' || *p == 'E') {
        int exsign = 1;
        int expr = 0;

        p++;
        if (*p == '-') {
            exsign = -1;
            p++;
        } else if (*p == '+') {
            p++;
        }
        while (isdigit(*p)) {
            expr = expr * 10 + (*p - '0');
            if (expr > SCALE_MAX) {
                return 0;
            }
            p++;
        }
        scale += expr * exsign;
        if (scale < SCALE_MIN || scale > SCALE_MAX) {
            return 0;
        }
    }
    if (end != NULL) {
        *end = p;
    }

    // 指数部の調整
    if (scale > 0) {
        BigInt ex;

        BigInt_init(&ex);
        int16_BigInt(&ex, 10);
        if (!BigInt_pow(&ex, scale)) {
            BigInt_close(&ex);
            return 0;
        }
        if (!BigInt_mul(&rat[0], &rat[0], &ex)) {
            BigInt_close(&ex);
            return 0;
        }
        int16_BigInt(&rat[1], 1);
    } else if (scale < 0) {
        int16_BigInt(&rat[1], 10);
        if (!BigInt_pow(&rat[1], -scale)) {
            return 0;
        }
    } else {
        int16_BigInt(&rat[1], 1);
    }

    BigRat_fix(rat);
    if (rat[0].size > 0) {
        rat[0].sign = sign;
    }
    return 1;
}
int double_BigRat(BigInt *rat, double d)
{
    enum {
        EXP_OFFSET = 53,
    };
    double v;
    int ex;
    uint64_t ival;

    if (d == 0.0 || isnan(d) || isinf(d)) {
        int16_BigInt(&rat[0], 0);
        int16_BigInt(&rat[0], 1);
        return d == 0.0;
    }
    v = (d < 0 ? -d : d);

    // IEEE754決め打ち
    memcpy(&ival, &v, 8);
    ex = ((ival >> 52) & 0x7ff) - 1022;
    ival &=  0xFffffFFFFffffLL;
    ival |= 0x10000000000000LL;

    int64_BigInt(&rat[0], ival);
    int16_BigInt(&rat[1], 1);

    if (ex < EXP_OFFSET) {
        BigInt_lsh(&rat[1], EXP_OFFSET - ex);
    } else if (ex > EXP_OFFSET) {
        BigInt_lsh(&rat[0], ex - EXP_OFFSET);
    }
    BigRat_fix(rat);
    if (d < 0) {
        rat[0].sign = -1;
    } else if (d > 0) {
        rat[0].sign = 1;
    } else {
        rat[0].sign = 0;
    }
    return 1;
}

char *BigRat_tostr_sub(int sign, BigInt *mi, BigInt *rem, int width_f)
{
    int len, len2;

    char *c_buf = malloc(BigInt_str_bufsize(mi, 10) + width_f + 3);
    if (mi->sign == 0 && sign == -1) {
        strcpy(c_buf, "-0");
    } else {
        BigInt_str(mi, 10, c_buf, FALSE);
    }
    if (width_f == 0) {
        return c_buf;
    }
    len = strlen(c_buf);
    c_buf[len++] = '.';
    BigInt_str(rem, 10, c_buf + len, FALSE);

    len2 = strlen(c_buf + len);
    if (len2 < width_f) {
        int d = width_f - len2;
        int i;
        // 123 -> 0123 (\0も移動する)
        for (i = len2; i >= 0; i--) {
            c_buf[len + d + i] = c_buf[len + i];
        }
        memset(c_buf + len, '0', d);
    }

    return c_buf;
}
/**
 * 10進数文字列表現
 * max_frac : 小数部の最大桁数 / -1:循環小数はエラー
 */
char *BigRat_str(BigInt *rat, int max_frac)
{
    char *c_buf = NULL;
    BigInt mi, rem;

    BigInt_init(&mi);
    BigInt_init(&rem);
    BigInt_divmod(&mi, &rem, &rat[0], &rat[1]);
    rem.sign = (rem.sign < 0) ? 1 : rem.sign;

    // 小数部がない場合、整数部のみ出力
    if (rem.sign == 0) {
        c_buf = malloc(BigInt_str_bufsize(&mi, 10) + 1);
        BigInt_str(&mi, 10, c_buf, FALSE);
    } else {
        int recr[2];
        int n_ex;
        BigInt ex;

        if (BigInt_get_recurrence(&rat[1], recr, max_frac + 16)) {
            if (max_frac < 0) {
                return NULL;
            }
            n_ex = max_frac;
        } else {
            n_ex = recr[0];
            if (n_ex > max_frac) {
                n_ex = max_frac;
            }
        }
        BigInt_init(&ex);
        int64_BigInt(&ex, 10);

        BigInt_pow(&ex, n_ex);     // ex = ex ** width_f
        BigInt_mul(&rem, &rem, &ex);
        BigInt_divmod(&rem, NULL, &rem, &rat[1]);

        c_buf = BigRat_tostr_sub(rat[0].sign, &mi, &rem, n_ex);
        BigInt_close(&ex);
    }
    return c_buf;
}
