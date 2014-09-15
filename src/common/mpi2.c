/*
 * mpi_sub.c
 *
 *  Created on: 2010/03/14
 *      Author: frog
 */

#include "mpi2.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>


void s_mp_clamp(mp_int *mp);
mp_err   s_mp_pad(mp_int *mp, mp_size min);
mp_err s_mp_add_d(mp_int *mp, mp_digit d); /* unsigned digit addition */
mp_err s_mp_mul_2d(mp_int *mp, mp_digit d); /* multiply by 2^d in place*/

/* Some things from the guts of the MPI library we make use of... */
mp_err s_mp_lshd(mp_int *mp, mp_size p);
void s_mp_rshd(mp_int *mp, mp_size p);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t mp2_get_hash(mp_int *mp)
{
	int i;
	uint32_t h = 0;

	ARGCHK(mp != NULL, MP_BADARG);

	for (i = 0; i < mp->used; i++) {
		h = h * 31 + mp->dp[i];
	}
	return h & LONG_MAX;
}

mp_err mp2_set_uint64(mp_int *mp, uint64_t z)
{
	int ix;
	mp_err res;

	ARGCHK(mp != NULL, MP_BADARG);

	for (ix = sizeof(uint64_t) - 1; ix >= 0; ix--) {
		if ((res = s_mp_mul_2d(mp, CHAR_BIT)) != MP_OKAY) {
			return res;
		}

		if ((res = s_mp_add_d(mp, (mp_digit) ((z >> (ix * CHAR_BIT)) & UCHAR_MAX))) != MP_OKAY) {
			return res;
		}
	}
	return MP_OKAY;
}

mp_err mp2_set_int64(mp_int *mp, int64_t z)
{
	mp_err res;

	ARGCHK(mp != NULL, MP_BADARG);

	if (z == 0LL) {
		mp_zero(mp);
		return MP_OKAY; /* shortcut for zero */
	}
	if (z == INT64_MIN) {
		mp_zero(mp);
		if ((res = mp_set_int(mp, 1)) != MP_OKAY) {
			return res;
		}
		if ((res = s_mp_mul_2d(mp, 63)) != MP_OKAY) {
			return res;
		}
		mp->sign = MP_NEG;
		return MP_OKAY;
	} else {
		uint64_t v = (z < 0 ? -z : z);
		if ((res = mp2_set_uint64(mp, v)) != MP_OKAY) {
			return res;
		}
		if (z < 0) {
			mp->sign = MP_NEG;
		}
	}

	return MP_OKAY;
}

mp_err mp2_set_double(mp_int *mp, double z)
{
	enum {
		EXP_OFFSET = 53,
	};
	int ex;
	union {
		double v;
		uint64_t ival;
	} u;
	u.v = (z < 0 ? -z : z);

	// IEEE754決め打ち
	ex = ((u.ival >> 52) & 0x7ff) - 1022;
	u.ival &=  0xFffffFFFFffffLL;
	u.ival |= 0x10000000000000LL;

	mp2_set_int64(mp, u.ival);
	if (ex < EXP_OFFSET) {
		mpl_rsh(mp, mp, EXP_OFFSET - ex);
	} else if (ex > EXP_OFFSET) {
		mpl_lsh(mp, mp, ex - EXP_OFFSET);
	}

	if (z < 0)
		mp->sign = MP_NEG;
	return MP_OKAY;
}
long mp2_tolong(mp_int *mp)
{
	long tmp = 0;
	int used = mp->used;

	if (used >= 1) {
		tmp = mp->dp[0];
		if (used >= 2) {
			tmp |= mp->dp[1] << 16;
		}
	}
	if (mp->sign == MP_NEG) {
		tmp = -tmp;
	}

	return tmp;
}
mp_err mp2_toint64(mp_int *mp, int64_t *ret)
{
	int64_t tmp = 0;
	int used = mp->used;
	int i;

	if (used > 4) {
		return MP_RANGE;
	}
	for (i = 0; i < used; i++) {
		tmp |= (int64_t)mp->dp[i] << (16 * i);
	}
	if (tmp < 0LL) {
		return MP_RANGE;
	}
	if (mp->sign == MP_NEG) {
		tmp = -tmp;
	}
	*ret = tmp;

	return MP_OKAY;
}
double mp2_todouble(mp_int *mp)
{
	int used = mp->used;
	double result = 0.0;
	uint64_t ival = 0LL;
	int ex = used * MP_DIGIT_BIT;
	int i;

	// 上位53ビットを取り出す
	for (i = 0; i < 4; i++) {
		ival <<= MP_DIGIT_BIT;
		if (i < used) {
			ival |= mp->dp[used - i - 1];
		}
	}
	if (ival == 0LL)
		return 0.0;

	// 52bit目が立つように調整する
	if ((ival & 0xFFF0000000000000LL) == 0LL) {
		int bits = 0;
		while ((ival & 0xFFF0000000000000LL) == 0LL) {
			ival <<= 1;
			bits++;
		}
		// 下に隠れたビットを補う
		if (used - 5 >= 0) {
			mp_digit d = mp->dp[used - 5];
			d >>= MP_DIGIT_BIT - bits;
			ival |= d;
		}
		ex -= bits;
	} else {
		while ((ival & 0xFFE0000000000000LL) != 0LL) {
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

	if (mp->sign == MP_NEG)
		result = -result;
	return result;
}

// 123
// 1.23
// 1e4
// 1.23e-15
mp_err mp2_read_rational(mp_int *md, const char *src, const char **end)
{
	const char *p = src;
	int scale = 0;
	mp_int ex;

	// 数値以外の文字が出現するまで進める
	if (*p == '+' || *p == '-')
		p++;
	while (isdigit(*p))
		p++;

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
		mp_read_radix(&md[0], (unsigned char*)c_buf, 10);

		free(c_buf);
	} else {
		mp_read_radix(&md[0], (unsigned char*)src, 10);
	}

	// 指数部(存在する場合)
	if (*p == 'e' || *p == 'E') {
		int sign = 1;
		int expr = 0;

		p++;
		if (*p == '-') {
			sign = -1;
			p++;
		} else if (*p == '+') {
			p++;
		}
		while (isdigit(*p)) {
			expr = expr * 10 + (*p - '0');
			if (expr > SCALE_MAX)
				return MP_BADARG;
			p++;
		}
		scale += expr * sign;
		if (scale < SCALE_MIN || scale > SCALE_MAX)
			return MP_RANGE;
	}
	if (end != NULL)
		*end = p;

	// 指数部の調整
	mp_init(&ex);
	mp_set(&ex, 10);
	if (scale > 0) {
		mp_expt_d(&ex, scale, &ex);
		mp_mul(&md[0], &ex, &md[0]);
		mp_set(&md[1], 1);
	} else if (scale < 0) {
		mp_expt_d(&ex, -scale, &md[1]);
	} else {
		mp_set(&md[1], 1);
	}
	mp_clear(&ex);

	mp2_fix_rational(&md[0], &md[1]);

	return MP_OKAY;
}

// 123
// 1.23
// 1e4
// 1.23e-15
mp_err mp2_double_to_rational(mp_int *md, double d)
{
	enum {
		EXP_OFFSET = 53,
	};
	double v;
	int ex;
	uint64_t ival;

	if (d == 0.0 || isnan(d) || isinf(d)) {
		mp_set(&md[0], 0);
		mp_set(&md[1], 1);
		return MP_OKAY;
	}
	v = (d < 0 ? -d : d);

	// IEEE754決め打ち
	memcpy(&ival, &v, 8);
	ex = ((ival >> 52) & 0x7ff) - 1022;
	ival &=  0xFffffFFFFffffLL;
	ival |= 0x10000000000000LL;

	mp2_set_int64(&md[0], ival);
	mp_set(&md[1], 1);

	if (ex < EXP_OFFSET) {
		mpl_lsh(&md[1], &md[1], EXP_OFFSET - ex);
	} else if (ex > EXP_OFFSET) {
		mpl_lsh(&md[0], &md[0], ex - EXP_OFFSET);
	}
	mp2_fix_rational(&md[0], &md[1]);
	if (d < 0) {
		md[0].sign = MP_NEG;
	}

	return MP_OKAY;
}

// 有理数を通分
mp_err mp2_fix_rational(mp_int *mp1, mp_int *mp2)
{
	mp_int gcd;

	mp_init(&gcd);
	mp_gcd(mp1, mp2, &gcd);
	gcd.sign = MP_ZPOS;

	// 1より大きい場合のみ、最大公約数で割る
	if (mp_cmp_d(&gcd, 1) > 0) {
		mp_int rem;
		mp_init(&rem);
		mp_div(mp1, &gcd, mp1, &rem);
		mp_div(mp2, &gcd, mp2, &rem);
	}

	// 符号はmd[0]だけに付ける
	if (mp2->sign == MP_NEG) {
		mp2->sign = MP_ZPOS;

		// numeratorの符号を逆にする
		if (mp1->sign == MP_NEG) {
			mp1->sign = MP_ZPOS;
		} else {
			mp1->sign = MP_NEG;
		}
	}

	return MP_OKAY;
}
