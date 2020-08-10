#include "fox_vm.h"
#include "bigint.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stddef.h>


#define Value_bigint(v) (((RefInt*)(intptr_t)(v))->bi)

/**
 * BigIntがint32の範囲内に収まっている場合は
 * VALUE_INT に変換する
 */
void fix_bigint(Value *v, BigInt *bi)
{
    // 絶対値が 0x4000 0000 より小さいか
    if (bi->size < 2 || (bi->size == 2 && (bi->d[1] & 0x8000) == 0)) {
        int32_t ret = BigInt_int32(bi);
        unref(*v);
        *v = int32_Value(ret);
    }
}

/**
 * 文字列が、base進数の文字で構成されているかを調べる
 */
static int is_valid_integer(const char *p, int base)
{
    if (*p == '+' || *p == '-') {
        p++;
    }
    if (*p == '\0') {
        return FALSE;
    }

    if (base <= 10) {
        while (*p != '\0') {
            if (*p < '0' || *p >= (base + '0')) {
                return FALSE;
            }
            p++;
        }
    } else {
        while (*p != '\0') {
            if (!isdigit(*p)) {
                if (*p < 'A') {
                    return FALSE;
                } else if (*p >= ('A' + base - 10) && *p < 'a') {
                    return FALSE;
                } else if (*p >= ('a' + base - 10)) {
                    return FALSE;
                }
            }
            p++;
        }
    }
    return TRUE;
}
/**
 * Int型へのゆるい変換
 * 変換できなければ0
 */
static int integer_new(Value *vret, Value *v, RefNode *node)
{
    if (fg->stk_top > v + 1) {
        Value v1 = v[1];
        const RefNode *type = Value_type(v1);

        if (type == fs->cls_float) {
            // Real -> Int
            double d = Value_float2(v1);

            if (isinf(d)) {
                *vret = int32_Value(0);
            } else if (d <= (double)INT32_MAX && d >= -(double)INT32_MAX) {
                *vret = int32_Value((int32_t)d);
            } else {
                RefInt *m1 = buf_new(fs->cls_int, sizeof(RefInt));
                *vret = vp_Value(m1);
                BigInt_init(&m1->bi);
                double_BigInt(&m1->bi, d);
            }
        } else if (type == fs->cls_str || type == fs->cls_bytes) {
            RefStr *rs = Value_vp(v1);

            if (is_valid_integer(rs->c, 10)) {
                long val;
                errno = 0;
                val = strtol(rs->c, NULL, 10);
                if (errno != 0 || val <= INT32_MIN || val > INT32_MAX) {
                    RefInt *m1 = buf_new(fs->cls_int, sizeof(RefInt));
                    *vret = vp_Value(m1);
                    BigInt_init(&m1->bi);
                    if (!cstr_BigInt(&m1->bi, 10, rs->c, rs->size)) {
                        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
                        return FALSE;
                    }
                } else {
                    *vret = int32_Value((int32_t)val);
                }
            } else {
                *vret = int32_Value(0);
            }
        } else if (type == fs->cls_frac) {
            BigInt rem;
            RefFrac *md = Value_vp(v1);
            RefInt *m1 = buf_new(fs->cls_int, sizeof(RefInt));
            *vret = vp_Value(m1);

            BigInt_init(&m1->bi);
            BigInt_init(&rem);

            BigInt_divmod(&m1->bi, &rem, &md->bi[0], &md->bi[1]);
            BigInt_close(&rem);

            fix_bigint(vret, &m1->bi);
        } else if (type == fs->cls_int) {
            // そのまま
            *vret = Value_cp(v1);
        } else if (type == fs->cls_bool) {
            // Null -> 0
            // true -> 1, false -> 0
            *vret = int32_Value(Value_bool(v1) ? 1 : 0);
        } else if (type == fs->cls_char) {
            *vret = int32_Value(Value_integral(v1));
        } else {
            // 未知の型
            *vret = int32_Value(0);
        }
    } else {
        *vret = int32_Value(0);
    }

    return TRUE;
}
/**
 * 基数を指定したStr -> Int変換
 */
static int integer_parse(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    int32_t base;
    long ret;
    RefStr *rs;

    if (fg->stk_top > v + 2) {
        Value v2 = v[2];
        base = Value_int64(v2, NULL);
        if (base < 2 || base > 36) {
            throw_errorf(fs->mod_lang, "ValueError", "Base range error (2 - 36 excepted)");
            return FALSE;
        }
    } else {
        base = 10;
    }

    rs = Value_vp(v1);
    if (str_has0(rs->c, rs->size) || !is_valid_integer(rs->c, base)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid integer string %q", rs->c);
        return FALSE;
    }

    errno = 0;
    ret = strtol(rs->c, NULL, base);
    if (errno != 0 || ret <= INT32_MIN || ret > INT32_MAX) {
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);
        BigInt_init(&mp->bi);
        if (!cstr_BigInt(&mp->bi, base, rs->c, rs->size)) {
            throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }
    } else {
        *vret = int32_Value(ret);
    }

    return TRUE;
}
static uint32_t dp_to_int32(uint8_t *dp, int n)
{
    uint32_t ret = 0LL;
    int i;

    for (i = 0; i < n; i++) {
        int m = (n - i - 1) * 16;
        ret |= (dp[0] << (m + 8)) | (dp[1] << m);
        dp += 2;
    }
    return ret;
}
static int read_int16_array(uint16_t *data, int n, Value r)
{
    int i;
    for (i = 0; i < n; i++) {
        uint8_t buf[2];
        int rd_size = 2;

        if (!stream_read_data(r, NULL, (char*)buf, &rd_size, FALSE, TRUE)) {
            return FALSE;
        }
        data[n - i - 1] = (buf[0] << 8) | buf[1];
    }
    return TRUE;
}
static int integer_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    char minus;
    uint32_t digits;
    int rd_size;

    rd_size = 1;
    if (!stream_read_data(r, NULL, &minus, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    if (!stream_read_uint32(r, &digits)) {
        return FALSE;
    }
    if (digits > 0xffff) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        return FALSE;
    }
    if (digits == 0) {
        *vret = int32_Value(0);
    } else if (digits <= 2) {
        uint8_t data[8];
        rd_size = digits * 2;
        if (!stream_read_data(r, NULL, (char*)data, &rd_size, FALSE, TRUE)) {
            return FALSE;
        }
        if (digits == 2 && (data[0] & 0x80) != 0) {
            int i;
            RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
            *vret = vp_Value(mp);

            BigInt_init(&mp->bi);
            BigInt_reserve(&mp->bi, digits);
            for (i = 0; i < digits; i++) {
                mp->bi.d[i] = (data[i * 2] << 8) | data[i * 2 + 1];
            }
            mp->bi.size = digits;
            if (minus) {
                mp->bi.sign = -1;
            } else {
                mp->bi.sign = 1;
            }
        } else {
            int32_t ival = dp_to_int32(data, digits);
            if (minus) {
                ival = -ival;
            }
            *vret = int32_Value(ival);
        }
    } else {
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);
        BigInt_init(&mp->bi);
        if (!BigInt_reserve(&mp->bi, digits)) {
            throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }
        if (!read_int16_array(mp->bi.d, digits, r)) {
            return FALSE;
        }
        mp->bi.size = digits;
        if (minus) {
            mp->bi.sign = -1;
        } else {
            mp->bi.sign = 1;
        }
    }

    return TRUE;
}
static int int32_to_dp(uint16_t *dp, uint32_t ival)
{
    int i;
    for (i = 0; i < 2 && ival != 0ULL; i++) {
        *dp++ = ival & 0xFFFF;
        ival >>= 16;
    }
    return i;
}
/**
 * BigEndianで書き出す
 */
static int write_int16_array(Value w, uint16_t *data, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        uint16_t s = data[n - i - 1];
        uint8_t buf[2];
        buf[0] = s >> 8;
        buf[1] = s & 0xFF;
        if (!stream_write_data(w, (char*)buf, 2)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int integer_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    char minus = 0;
    int32_t digits;
    uint16_t *data = NULL;
    uint16_t dp[4];

    if (Value_isref(*v)) {
        RefInt *mp = Value_vp(*v);
        if (mp->bi.sign < 0) {
            minus = 1;
        }
        digits = mp->bi.size;
        data = mp->bi.d;
    } else {
        int32_t ival = Value_integral(*v);
        if (ival < 0) {
            minus = 1;
            ival = -ival;
        }
        digits = int32_to_dp(dp, ival);
        data = dp;
    }

    if (!stream_write_data(w, &minus, 1)) {
        return FALSE;
    }
    if (!stream_write_uint32(w, digits)) {
        return FALSE;
    }
    if (!write_int16_array(w, data, digits)) {
        return FALSE;
    }

    return TRUE;
}
static int integer_dispose(Value *vret, Value *v, RefNode *node)
{
    RefInt *mp = Value_vp(*v);
    BigInt_close(&mp->bi);
    return TRUE;
}
static int integer_eq(Value *vret, Value *v, RefNode *node)
{
    Value v0 = *v;
    Value v1 = v[1];
    int eq;

    if (Value_isref(v0)) {
        if (Value_isref(v1)) {
            RefInt *m1 = Value_vp(v0);
            RefInt *m2 = Value_vp(v1);
            eq = (BigInt_cmp(&m1->bi, &m2->bi) == 0);
        } else {
            eq = FALSE;
        }
    } else {
        if (Value_isref(v1)) {
            eq = FALSE;
        } else {
            eq = (v0 == v1);
        }
    }
    *vret = bool_Value(eq);

    return TRUE;
}
static int integer_cmp(Value *vret, Value *v, RefNode *node)
{
    Value v0 = *v;
    Value v1 = v[1];
    int cmp;

    if (Value_isref(v0)) {
        if (Value_isref(v1)) {
            RefInt *m1 = Value_vp(v0);
            RefInt *m2 = Value_vp(v1);
            cmp = BigInt_cmp(&m1->bi, &m2->bi);
        } else {
            RefInt *m1 = Value_vp(v0);
            // always |v0| > |v1|
            cmp = m1->bi.sign;
        }
    } else {
        if (Value_isref(v1)) {
            RefInt *m2 = Value_vp(v1);
            // always |v0| > |v1|
            cmp = -m2->bi.sign;
        } else {
            int i0 = Value_integral(v0);
            int i1 = Value_integral(v1);
            if (i0 < i1) {
                cmp = -1;
            } else if (i0 > i1) {
                cmp = 1;
            } else {
                cmp = 0;
            }
        }
    }
    *vret = int32_Value(cmp);

    return TRUE;
}
/**
 * add : vp == 0
 * sub : vp == 1
 */
static int integer_addsub(Value *vret, Value *v, RefNode *node)
{
    Value v0 = *v;
    Value v1 = v[1];
    int sub = FUNC_INT(node);

    if (Value_isint(v0) && Value_isint(v1)) {
        int64_t ret = Value_integral(v0);
        if (sub) {
            ret -= Value_integral(v1);
        } else {
            ret += Value_integral(v1);
        }
        if (ret < -INT32_MAX || ret > INT32_MAX) {
            *vret = int64_Value(ret);
        } else {
            *vret = int32_Value(ret);
        }
    } else {
        BigInt m1;
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);

        if (Value_isint(v0)) {
            BigInt_init(&mp->bi);
            int64_BigInt(&mp->bi, Value_integral(v0));
        } else {
            BigInt_copy(&mp->bi, &Value_bigint(v0));
        }
        if (Value_isint(v1)) {
            BigInt_init(&m1);
            int64_BigInt(&m1, Value_integral(v1));
        } else {
            m1 = Value_bigint(v1);
        }
        if (sub) {
            BigInt_sub(&mp->bi, &m1);
        } else {
            BigInt_add(&mp->bi, &m1);
        }
        if (Value_isint(v1)) {
            BigInt_close(&m1);
        }
        fix_bigint(vret, &mp->bi);
    }
    return TRUE;
}
/*
void dump_i(int i, int nl)
{
    fprintf(stderr, "%d%c", i, (nl ? '\n' : ','));
}
void dump_mp(mp_int *mp, int nl)
{
    char *c_buf = malloc(mp_radix_size(mp, 10) + 2);
    mp_toradix(mp, (unsigned char *)c_buf, 10);
    fprintf(stderr, "%s%c", c_buf, (nl ? '\n' : ','));
    free(c_buf);
}
*/
static int integer_mul(Value *vret, Value *v, RefNode *node)
{
    Value v0 = *v;
    Value v1 = v[1];

    if (Value_isint(v0) && Value_isint(v1)) {
        int64_t ret = Value_integral(v0);
        ret *= Value_integral(v1);
        if (ret < -INT32_MAX || ret > INT32_MAX) {
            *vret = int64_Value(ret);
        } else {
            *vret = int32_Value(ret);
        }
    } else {
        BigInt m0;
        BigInt m1;
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);

        if (Value_isint(v0)) {
            BigInt_init(&m0);
            int64_BigInt(&m0, Value_integral(v0));
        } else {
            m0 = Value_bigint(v0);
        }
        if (Value_isint(v1)) {
            BigInt_init(&m1);
            int64_BigInt(&m1, Value_integral(v1));
        } else {
            m1 = Value_bigint(v1);
        }
        BigInt_mul(&mp->bi, &m0, &m1);
        if (Value_isint(v0)) {
            BigInt_close(&m0);
        }
        if (Value_isint(v1)) {
            BigInt_close(&m1);
        }
        if (mp->bi.sign == 0) {
            unref(*vret);
            *vret = int32_Value(0);
        }
    }
    return TRUE;
}
static int integer_div(Value *vret, Value *v, RefNode *node)
{
    Value v0 = *v;
    Value v1 = v[1];
    int modulo = FUNC_INT(node);

    if (Value_isint(v0) && Value_isint(v1)) {
        int32_t i1 = Value_integral(v0);
        int32_t i2 = Value_integral(v1);

        if (i2 == 0) {
            throw_errorf(fs->mod_lang, "ZeroDivisionError", "%s", (modulo ? "Int % 0" : "Int / 0"));
            return FALSE;
        }

        // 負の数の除数をpythonに合わせるため、ごにょごにょする
        if (modulo) {
            if ((i1 < 0 && i2 > 0) || (i1 > 0 && i2 < 0)) {
                int32_t m = i1 % i2;
                if (m != 0) {
                    m += i2;
                }
                *vret = int32_Value(m);
            } else {
                *vret = int32_Value(i1 % i2);
            }
        } else {
            if (i1 < 0 && i2 > 0) {
                int32_t d = (i1 + 1) / i2;
                *vret = int32_Value(d - 1);
            } else if (i1 > 0 && i2 < 0) {
                int32_t d = (i1 - 1) / i2;
                *vret = int32_Value(d - 1);
            } else {
                *vret = int32_Value(i1 / i2);
            }
        }
    } else {
        // mp_intに合わせる
        RefInt *mp;
        BigInt m0, m1;
        BigInt mdiv, mmod;

        if (Value_isint(v0)) {
            BigInt_init(&m0);
            int64_BigInt(&m0, Value_integral(v0));
        } else {
            m0 = Value_bigint(v0);
        }
        if (Value_isint(v1)) {
            BigInt_init(&m1);
            int64_BigInt(&m1, Value_integral(v1));
        } else {
            m1 = Value_bigint(v1);
        }

        BigInt_init(&mdiv);
        BigInt_init(&mmod);
        BigInt_divmod(&mdiv, &mmod, &m0, &m1);

        // 負の数の除数をpython方式に合わせるため、ごにょごにょする
        mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);
        if (modulo) {
            if ((m0.sign < 0 && m1.sign > 0) || (m0.sign > 0 && m1.sign < 0)) {
                if (mmod.sign != 0) {
                    BigInt_add(&mmod, &m1);
                }
            }
            mp->bi = mmod;
            BigInt_close(&mdiv);
            fix_bigint(vret, &mmod);
        } else {
            if ((m0.sign < 0 && m1.sign > 0) || (m0.sign > 0 && m1.sign < 0)) {
                if (mdiv.sign != 0) {
                    BigInt_add_d(&mdiv, -1);
                }
            }
            mp->bi = mdiv;
            BigInt_close(&mmod);
            fix_bigint(vret, &mdiv);
        }
        if (Value_isint(v0)) {
            BigInt_close(&m0);
        }
        if (Value_isint(v1)) {
            BigInt_close(&m1);
        }
    }
    return TRUE;
}
/**
 * 符号反転
 */
static int integer_negative(Value *vret, Value *v, RefNode *node)
{
    if (Value_isref(*v)) {
        RefInt *src = Value_vp(*v);
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);

        BigInt_init(&mp->bi);
        BigInt_copy(&mp->bi, &src->bi);
        mp->bi.sign *= -1;
    } else {
        *vret = int32_Value(-Value_integral(*v));
    }
    return TRUE;
}
/**
 * +1 , -1
 */
static int integer_inc(Value *vret, Value *v, RefNode *node)
{
    int inc = FUNC_INT(node);

    if (Value_isint(*v)) {
        int64_t ret = Value_integral(*v);
        ret += inc;

        if (ret < -INT32_MAX || ret > INT32_MAX) {
            RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
            *vret = vp_Value(mp);
            BigInt_init(&mp->bi);
            int64_BigInt(&mp->bi, ret);
        } else {
            *vret = int32_Value(ret);
        }
    } else {
        RefInt *m1 = Value_vp(*v);
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);

        BigInt_init(&mp->bi);
        BigInt_copy(&mp->bi, &m1->bi);
        BigInt_add_d(&mp->bi, inc);
    }
    return TRUE;
}
static int integer_logical(Value *vret, Value *v, RefNode *node)
{
    int type = FUNC_INT(node);
    Value v0 = *v;
    Value v1 = v[1];

    if (Value_isint(v0) && Value_isint(v1)) {
        int32_t i1 = Value_integral(v0);
        int32_t i2 = Value_integral(v1);
        int32_t ret = 0;

        switch (type) {
        case T_AND:
            ret = i1 & i2;
            break;
        case T_OR:
            ret = i1 | i2;
            break;
        case T_XOR:
            ret = i1 ^ i2;
            break;
        }
        if (ret == INT32_MIN) {
            *vret = int64_Value(INT32_MIN);
        } else {
            *vret = int32_Value(ret);
        }
    } else {
        BigInt m1;
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);

        if (Value_isint(v0)) {
            BigInt_init(&mp->bi);
            int64_BigInt(&mp->bi, Value_integral(v0));
        } else {
            BigInt_copy(&mp->bi, &Value_bigint(v0));
        }
        if (Value_isint(v1)) {
            BigInt_init(&m1);
            int64_BigInt(&m1, Value_integral(v1));
        } else {
            m1 = Value_bigint(v1);
        }
        switch (type) {
        case T_AND:
            BigInt_bitop(&mp->bi, &m1, BIGINT_OP_AND);
            break;
        case T_OR:
            BigInt_bitop(&mp->bi, &m1, BIGINT_OP_OR);
            break;
        case T_XOR:
            BigInt_bitop(&mp->bi, &m1, BIGINT_OP_XOR);
            break;
        }
        if (Value_isint(v1)) {
            BigInt_close(&m1);
        }
        fix_bigint(vret, &mp->bi);
    }
    return TRUE;
}
static int get_msb_pos(int32_t val)
{
    int i;

    for (i = 30; i >= 0; i--) {
        if (((1LL << i) & val) != 0) {
            return i + 1;
        }
    }
    return 0;
}
/**
 * FALSE:左シフト
 * TRUE:右シフト
 */
static int integer_shift(Value *vret, Value *v, RefNode *node)
{
    int shift = Value_integral(v[1]);

    if (Value_isref(v[1]) || shift < -32767 || shift > 32767) {
        throw_errorf(fs->mod_lang, "ValueError", "shift operand out of range(-32767 - 32767)");
        return FALSE;
    }
    if (FUNC_INT(node)) {
        shift = -shift;
    }

    if (Value_isint(*v)){
        int32_t i1 = Value_integral(*v);
        if (i1 == 0) {
            *vret = int32_Value(0);
        } if (shift + get_msb_pos(i1) > 31) {
            RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
            *vret = vp_Value(mp);
            BigInt_init(&mp->bi);
            int64_BigInt(&mp->bi, i1);
            BigInt_lsh(&mp->bi, shift);
        } else if (shift >= 0) {
            int32_t ret = i1 << shift;
            if (ret == INT32_MIN) {
                *vret = int64_Value(INT32_MIN);
            } else {
                *vret = int32_Value(ret);
            }
        } else {
            *vret = int32_Value(i1 >> (-shift));
        }
    } else {
        RefInt *mp = Value_vp(*v);

        if (shift > 0) {
            RefInt *mp2 = buf_new(fs->cls_int, sizeof(RefInt));
            *vret = vp_Value(mp2);
            BigInt_init(&mp2->bi);
            BigInt_lsh(&mp->bi, shift);
        } else if (shift < 0) {
            RefInt *mp2 = buf_new(fs->cls_int, sizeof(RefInt));
            *vret = vp_Value(mp2);
            BigInt_init(&mp2->bi);
            if (mp->bi.sign < 0) {
                BigInt_add_d(&mp2->bi, -1);
                BigInt_rsh(&mp->bi, -shift);
                BigInt_add_d(&mp2->bi, 1);
            } else {
                BigInt_rsh(&mp->bi, -shift);
            }
            fix_bigint(vret, &mp2->bi);
        } else {
            *vret = *v;
            *v = VALUE_NULL;
        }
    }
    return TRUE;
}
/**
 * 2の補数
 */
static int integer_inv(Value *vret, Value *v, RefNode *node)
{
    if (Value_isref(*v)) {
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        RefInt *src = Value_vp(*v);
        BigInt_init(&mp->bi);
        BigInt_copy(&mp->bi, &src->bi);
        mp->bi.sign *= -1;
        BigInt_add_d(&mp->bi, -1);
        fix_bigint(vret, &mp->bi);
    } else {
        int32_t val = ~Value_integral(*v);
        if (val == INT32_MIN) {
            *vret = int64_Value(val);
        } else {
            *vret = int32_Value(val);
        }
    }
    return TRUE;
}
static int integer_sign(Value *vret, Value *v, RefNode *node)
{
    if (Value_isref(*v)) {
        RefInt *mp = Value_vp(*v);
        // 0は考慮する必要がない
        if (mp->bi.sign < 0) {
            *vret = int32_Value(-1);
        } else {
            *vret = int32_Value(1);
        }
    } else {
        int i = Value_integral(*v);
        if (i > 0) {
            *vret = int32_Value(1);
        } else if (i < 0) {
            *vret = int32_Value(-1);
        } else {
            *vret = int32_Value(0);
        }
    }
    return TRUE;
}
static int integer_tofloat(Value *vret, Value *v, RefNode *node)
{
    RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
    *vret = vp_Value(rd);

    if (Value_isint(*v)) {
        rd->d = (double)Value_integral(*v);
    } else {
        RefInt *mp = Value_vp(*v);
        rd->d = BigInt_double(&mp->bi);
    }

    return TRUE;
}
static int integer_tofrac(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));
    *vret = vp_Value(md);
    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);
    int64_BigInt(&md->bi[1], 1);  // 分母=1

    if (Value_isint(*v)) {
        int64_BigInt(&md->bi[0], Value_integral(*v));
    } else {
        RefInt *mp = Value_vp(*v);
        BigInt_copy(&md->bi[0], &mp->bi);
    }

    return TRUE;
}
static int integer_tochar(Value *vret, Value *v, RefNode *node)
{
    int code = -1;

    if (Value_isint(*v)) {
        code = Value_integral(*v);
    }
    if (code < 0 || code >= CODEPOINT_END || (code >= SURROGATE_U_BEGIN && code < SURROGATE_END)) {
        throw_errorf(fs->mod_lang, "ValueError", "Character range error (U+0000 - U+10FFFF)");
        return FALSE;
    }
    *vret = integral_Value(fs->cls_char, code);

    return TRUE;
}
static int integer_empty(Value *vret, Value *v, RefNode *node)
{
    if (Value_isint(*v)) {
        *vret = bool_Value(Value_integral(*v) == 0);
    } else {
        *vret = VALUE_FALSE;
    }

    return TRUE;
}
static int integer_hash(Value *vret, Value *v, RefNode *node)
{
    if (Value_isint(*v)) {
        int32_t i = Value_integral(*v);
        int32_t h = int32_hash(i);
        *vret = int32_Value(h);
    } else {
        RefInt *mp = Value_vp(*v);
        *vret = int32_Value(BigInt_hash(&mp->bi));
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

/**
 * 変換できないときは0dを返す
 */
static int frac_new(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));
    *vret = vp_Value(md);

    if (fg->stk_top > v + 2) {
        // Frac(1,2) -> 0.5d
        Value v1 = v[1];
        Value v2 = v[2];
        const RefNode *v1_type = Value_type(v1);

        // 第1引数は必ずint
        if (v1_type != fs->cls_int) {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_int, v1_type, 1);
            return FALSE;
        }

        BigInt_init(&md->bi[0]);
        BigInt_init(&md->bi[1]);
        if (Value_isint(v1)) {
            int64_BigInt(&md->bi[0], Value_integral(v1));
        } else {
            RefInt *m1 = Value_vp(v1);
            BigInt_copy(&md->bi[0], &m1->bi);
        }
        if (Value_isint(v2)) {
            int32_t i2 = Value_integral(v2);
            if (i2 == 0) {
                throw_errorf(fs->mod_lang, "ZeroDivisionError", "denominator == 0");
                return FALSE;
            }
            int64_BigInt(&md->bi[1], i2);
        } else {
            RefInt *m2 = Value_vp(v2);
            BigInt_copy(&md->bi[1], &m2->bi);
        }
        BigRat_fix(md->bi);
    } else if (fg->stk_top > v + 1) {
        Value v1 = v[1];
        const RefNode *type = Value_type(v1);

        if (type == fs->cls_frac) {
            // 自分自身を返す
            *vret = v1;
            v[1] = VALUE_NULL;
            return TRUE;
        }

        if (type == fs->cls_int) {
            BigInt_init(&md->bi[0]);
            BigInt_init(&md->bi[1]);
            if (Value_isint(v1)) {
                int64_BigInt(&md->bi[0], Value_integral(v1));
            } else {
                RefInt *m1 = Value_vp(v1);
                BigInt_copy(&md->bi[0], &m1->bi);
            }
            int64_BigInt(&md->bi[1], 1);  // 分母=1
        } else if (type == fs->cls_str || type == fs->cls_bytes) {
            RefStr *rs = Value_vp(v1);

            BigInt_init(&md->bi[0]);
            BigInt_init(&md->bi[1]);
            if (!cstr_BigRat(md->bi, rs->c, NULL)) {
                // 読み込みエラー時は0とする
                goto SET_ZERO;
            }
        } else if (type == fs->cls_float) {
            double d = Value_float2(v1);
            if (isinf(d)) {
                goto SET_ZERO;
            } else {
                BigInt_init(&md->bi[0]);
                BigInt_init(&md->bi[1]);
                double_BigRat(md->bi, d);
            }
        } else if (type == fs->cls_bool) {
            BigInt_init(&md->bi[0]);
            BigInt_init(&md->bi[1]);
            if (Value_bool(v1)) {
                int64_BigInt(&md->bi[0], 1);
            }
            int64_BigInt(&md->bi[1], 1);
        }
        // 未知の型の場合、0とする
    } else {
        // ゼロ初期化 (0 / 1)
        goto SET_ZERO;
    }
    return TRUE;

SET_ZERO:
    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);
    int64_BigInt(&md->bi[1], 1);
    return TRUE;
}
static int frac_parse(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));

    RefStr *rs = Value_vp(v1);
    const char *end;

    if (rs->size == 0 || str_has0(rs->c, rs->size)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid rational string %q", rs->c);
        return FALSE;
    }

    *vret = vp_Value(md);

    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);
    if (!cstr_BigRat(md->bi, rs->c, &end) || *end != '\0') {
        // 読み込みエラー時は例外
        throw_errorf(fs->mod_lang, "ParseError", "Invalid rational string %q", rs->c);
        return FALSE;
    }
    return TRUE;
}
static int frac_marshal_read(Value *vret, Value *v, RefNode *node)
{
    char minus;
    uint32_t digits;
    int rd_size = 1;
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];

    *vret = vp_Value(md);

    if (!stream_read_data(r, NULL, &minus, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }

    if (!stream_read_uint32(r, &digits)) {
        return FALSE;
    }
    BigInt_init(&md->bi[0]);
    if (!BigInt_reserve(&md->bi[0], digits)) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    if (!read_int16_array(md->bi[0].d, digits, r)) {
        return FALSE;
    }
    md->bi[0].size = digits;
    if (digits > 0) {
        if (minus) {
            md->bi[0].sign = -1;
        } else {
            md->bi[0].sign = 1;
        }
    } else {
        md->bi[0].sign = 0;
    }

    if (!stream_read_uint32(r, &digits)) {
        return FALSE;
    }
    BigInt_init(&md->bi[1]);
    if (!BigInt_reserve(&md->bi[1], digits)) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    if (!read_int16_array(md->bi[1].d, digits, r)) {
        return FALSE;
    }
    md->bi[1].size = digits;
    if (digits == 0) {
        throw_errorf(fs->mod_lang, "ZeroDivisionError", "denominator == 0");
        return FALSE;
    }
    md->bi[1].sign = 1;

    if (minus) {
        md->bi[0].sign = -1;
    }

    return TRUE;
}
static int frac_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    char minus = 0;
    RefFrac *md = Value_vp(*v);

    if (md->bi[0].sign < 0) {
        minus = 1;
    }
    if (!stream_write_data(w, &minus, 1)) {
        return FALSE;
    }

    if (!stream_write_uint32(w, md->bi[0].size)) {
        return FALSE;
    }
    if (!write_int16_array(w, md->bi[0].d, md->bi[0].size)) {
        return FALSE;
    }

    if (!stream_write_uint32(w, md->bi[1].size)) {
        return FALSE;
    }
    if (!write_int16_array(w, md->bi[1].d, md->bi[1].size)) {
        return FALSE;
    }

    return TRUE;
}
static int frac_dispose(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    BigInt_close(&md->bi[0]);
    BigInt_close(&md->bi[1]);

    return TRUE;
}
static int frac_empty(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    int empty = (md->bi[0].sign == 0);
    *vret = bool_Value(empty);
    return TRUE;
}
static int frac_hash(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    uint32_t hash = BigInt_hash(&md->bi[0]) ^ BigInt_hash(&md->bi[1]);
    *vret = int32_Value(hash & INT32_MAX);
    return TRUE;
}
static int frac_eq(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md1 = Value_vp(*v);
    RefFrac *md2 = Value_vp(v[1]);

    // eq(a/b, c/d) -> a == c && b == d
    int result = (BigInt_cmp(&md1->bi[0], &md2->bi[0]) == 0 && BigInt_cmp(&md1->bi[1], &md2->bi[1]) == 0);
    *vret = bool_Value(result);

    return TRUE;
}
static int frac_cmp(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md1 = Value_vp(*v);
    RefFrac *md2 = Value_vp(v[1]);
    int result;

    if (md1->bi[0].sign != md2->bi[0].sign) {
        result = md1->bi[0].sign - md2->bi[0].sign;
    } else {
        BigInt c1, c2;
        // cmp(a/b, c/d) -> cmp(a*d, c*b)
        BigInt_init(&c1);
        BigInt_init(&c2);
        BigInt_mul(&c1, &md1->bi[0], &md2->bi[1]);
        BigInt_mul(&c2, &md2->bi[0], &md1->bi[1]);

        result = BigInt_cmp(&c1, &c2);
        BigInt_close(&c1);
        BigInt_close(&c2);
    }
    *vret = int32_Value(result);

    return TRUE;
}
static int frac_negative(Value *vret, Value *v, RefNode *node)
{
    RefFrac *src = Value_vp(*v);
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));
    *vret = vp_Value(md);

    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);

    BigInt_copy(&md->bi[0], &src->bi[0]);
    BigInt_copy(&md->bi[1], &src->bi[1]);
    md->bi[0].sign *= -1;

    return TRUE;
}
static int frac_addsub(Value *vret, Value *v, RefNode *node)
{
    int sub = FUNC_INT(node);
    BigInt tmp;

    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));
    RefFrac *md1 = Value_vp(*v);
    RefFrac *md2 = Value_vp(v[1]);

    *vret = vp_Value(md);

    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);
    BigInt_init(&tmp);

    // (a/b) + (c/d) -> (a*d + b*c) / (b*d)
    // 分子
    BigInt_mul(&md->bi[0], &md1->bi[0], &md2->bi[1]);
    BigInt_mul(&tmp, &md2->bi[0], &md1->bi[1]);
    if (sub) {
        BigInt_sub(&md->bi[0], &tmp);
    } else {
        BigInt_add(&md->bi[0], &tmp);
    }
    BigInt_close(&tmp);

    // 分母
    BigInt_mul(&md->bi[1], &md1->bi[1], &md2->bi[1]);
    BigRat_fix(md->bi);

    return TRUE;
}
static int frac_mul(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));
    RefFrac *md1 = Value_vp(*v);
    RefFrac *md2 = Value_vp(v[1]);

    *vret = vp_Value(md);

    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);

    // (a/b) * (c/d) -> (a*c) / (b*d)
    BigInt_mul(&md->bi[0], &md1->bi[0], &md2->bi[0]);
    BigInt_mul(&md->bi[1], &md1->bi[1], &md2->bi[1]);

    BigRat_fix(md->bi);

    return TRUE;
}
static int frac_div(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));
    RefFrac *md1 = Value_vp(*v);
    RefFrac *md2 = Value_vp(v[1]);

    *vret = vp_Value(md);

    // ゼロ除算チェック
    if (md2->bi[0].sign == 0) {
        throw_errorf(fs->mod_lang, "ZeroDivisionError", "Frac / 0d");
        return FALSE;
    }

    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);

    // (a/b) / (c/d) -> (a*d) / (b*c)
    BigInt_mul(&md->bi[0], &md1->bi[0], &md2->bi[1]);
    BigInt_mul(&md->bi[1], &md1->bi[1], &md2->bi[0]);

    BigRat_fix(md->bi);

    return TRUE;
}

// vp = 0 : 分子を取り出す
// vp = 1 : 分母を取り出す
static int frac_get_part(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    int part = FUNC_INT(node);
    int32_t val = BigInt_int32(&md->bi[part]);

    if (val != INT32_MIN) {
        *vret = int32_Value(val);
    } else {
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);
        BigInt_init(&mp->bi);
        BigInt_copy(&mp->bi, &md->bi[part]);
    }

    return TRUE;
}
static int frac_sign(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    *vret = int32_Value(md->bi[0].sign);
    return TRUE;
}
// 有理数を0方向に切り捨てて整数型に変換する
static int frac_toint(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
    BigInt rem;

    *vret = vp_Value(mp);

    BigInt_init(&mp->bi);
    BigInt_init(&rem);

    BigInt_divmod(&mp->bi, &rem, &md->bi[0], &md->bi[1]);
    BigInt_close(&rem);
    fix_bigint(vret, &mp->bi);

    return TRUE;
}
// 有理数を浮動小数点に変換する
static int frac_tofloat(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
    double d1 = BigInt_double(&md->bi[0]);
    double d2 = BigInt_double(&md->bi[1]);

    *vret = vp_Value(rd);

    rd->d = d1 / d2;
    if (isnan(rd->d)) {
        throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
        return FALSE;
    }

    return TRUE;
}
static int frac_round(Value *vret, Value *v, RefNode *node)
{
    Value v0 = v[0];
    RefFrac *md = Value_vp(v0);
    RefFrac *mp;   // result
    BigInt mul, rem;
    int round_mode = parse_round_mode(Value_vp(v[1]));

    int sign = md->bi[0].sign;
    int64_t pred = 0;

    if (round_mode == ROUND_ERR) {
        throw_errorf(fs->mod_lang, "ValueError", "Unknown round mode %v", v[1]);
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        pred = Value_int64(v[2], NULL);
        if (pred < -32768 || pred > 32767) {
            throw_errorf(fs->mod_lang, "ValueError", "Illigal parameter #2 (-32768 - 32767)");
            return FALSE;
        }
    }

    mp = buf_new(fs->cls_frac, sizeof(RefFrac));
    *vret = vp_Value(mp);
    BigInt_init(&mp->bi[0]);
    BigInt_init(&mp->bi[1]);

    BigInt_copy(&mp->bi[0], &md->bi[0]);
    BigInt_copy(&mp->bi[1], &md->bi[1]);
    if (round_mode == ROUND_FLOOR) {
        if (sign < 0) {
            round_mode = ROUND_UP;
        } else {
            round_mode = ROUND_DOWN;
        }
    } else if (round_mode == ROUND_CEILING) {
        if (sign < 0) {
            round_mode = ROUND_DOWN;
        } else {
            round_mode = ROUND_UP;
        }
    }

    BigInt_init(&mul);
    if (pred < 0) {
        int64_BigInt(&mul, 10);
        BigInt_pow(&mul, -pred);
        BigInt_mul(&mp->bi[1], &mp->bi[1], &mul);
    } else if (pred > 0) {
        int64_BigInt(&mul, 10);
        BigInt_pow(&mul, pred);
        BigInt_mul(&mp->bi[0], &mp->bi[0], &mul);
    }

    BigInt_init(&rem);
    BigInt_divmod(&mp->bi[0], &rem, &mp->bi[0], &mp->bi[1]);
    if (rem.sign < 0) {
        rem.sign = -rem.sign;
    }

    switch (round_mode) {
    case ROUND_DOWN:
        break;
    case ROUND_UP:
        if (rem.sign != 0) {
            BigInt_add_d(&mp->bi[0], mp->bi[0].sign);
        }
        break;
    case ROUND_HALF_DOWN:
        fs->BigInt_lsh(&rem, 1);  // rem *= 2
        if (BigInt_cmp(&rem, &mp->bi[1]) > 0) {
            BigInt_add_d(&mp->bi[0], mp->bi[0].sign);
        }
        break;
    case ROUND_HALF_UP:
        BigInt_lsh(&rem, 1);  // rem *= 2
        if (BigInt_cmp(&rem, &mp->bi[1]) >= 0) {
            BigInt_add_d(&mp->bi[0], mp->bi[0].sign);
        }
        break;
    case ROUND_HALF_EVEN: {
        int cmp;
        BigInt_lsh(&rem, 1);  // rem *= 2
        cmp = BigInt_cmp(&rem, &mp->bi[1]);
        if (cmp == 0) {
            if (mp->bi[0].size > 0 && (mp->bi[0].d[0] & 1) != 0) {
                cmp = 1;
            }
        }
        if (cmp > 0) {
            BigInt_add_d(&mp->bi[0], mp->bi[0].sign);
        }
        break;
    }
    }
    if (pred < 0) {
        BigInt_mul(&mp->bi[0], &mp->bi[0], &mul);
        int64_BigInt(&mp->bi[1], 1);
    } else if (pred > 0) {
        BigInt_copy(&mp->bi[1], &mul);
    } else {
        int64_BigInt(&mp->bi[1], 1);
    }
    BigInt_close(&mul);
    BigInt_close(&rem);

    BigRat_fix(mp->bi);
    mp->bi[0].sign = sign;

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

/**
 * 変換できないときは0.0を返す
 */
static int float_new(Value *vret, Value *v, RefNode *node)
{
    if (fg->stk_top > v + 1) {
        Value v1 = v[1];
        const RefNode *type = Value_type(v1);

        if (type == fs->cls_int) {
            RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
            *vret = vp_Value(rd);
            if (Value_isint(v1)) {
                rd->d = (double)Value_integral(v1);
            } else {
                RefInt *mp = Value_vp(v1);
                rd->d = BigInt_double(&mp->bi);
                if (isinf(rd->d)) {
                    rd->d = 0.0;
                }
            }
        } else if (type == fs->cls_str || type == fs->cls_bytes) {
            RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
            RefStr *rs = Value_vp(v1);

            *vret = vp_Value(rd);
            errno = 0;
            rd->d = strtod(rs->c, NULL);
            if (errno != 0) {
                rd->d = 0.0;
            }
        } else if (type == fs->cls_float) {
            *vret = v1;
            v[1] = VALUE_NULL;
            return TRUE;
        } else if (type == fs->cls_frac) {
            RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
            RefFrac *md = Value_vp(v[1]);
            double d1 = BigInt_double(&md->bi[0]);
            double d2 = BigInt_double(&md->bi[1]);

            *vret = vp_Value(rd);
            rd->d = d1 / d2;
            if (isnan(rd->d)) {
                rd->d = 0.0;
            }
            return TRUE;
        } else if (type == fs->cls_bool) {
            RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
            *vret = vp_Value(rd);
            rd->d = Value_bool(v1) ? 1.0 : 0.0;
        }
    }

    return TRUE;
}
static int float_parse(Value *vret, Value *v, RefNode *node)
{
    RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
    RefStr *rs = Value_vp(v[1]);
    char *end = rs->c;

    if (rs->size == 0 || str_has0(rs->c, rs->size)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid float string %q", rs->c);
        return FALSE;
    }

    *vret = vp_Value(rd);
    errno = 0;
    rd->d = strtod(rs->c, &end);
    if (errno != 0) {
        throw_errorf(fs->mod_lang, "ValueError", "%s", strerror(errno));
        return FALSE;
    }
    if (*end != '\0') {
        // 読み込みエラー時は例外
        throw_errorf(fs->mod_lang, "ParseError", "Invalid float string %q", rs->c);
        return FALSE;
    }

    return TRUE;
}
int float_marshal_read(Value *vret, Value *v, RefNode *node)
{
    union {
        uint64_t i;
        double d;
    } u;
    uint8_t data[8];
    int rd_size = 8;
    int i;

    RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];

    *vret = vp_Value(rd);

    if (!stream_read_data(r, NULL, (char*)data, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }

    u.i = 0ULL;
    for (i = 0; i < 8; i++) {
        u.i |= ((uint64_t)data[i]) << ((7 - i) * 8);
    }
    if (isnan(u.d)) {
        throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
        return FALSE;
    }
    rd->d = u.d;
    return TRUE;
}
int float_marshal_write(Value *vret, Value *v, RefNode *node)
{
    union {
        uint64_t i;
        double d;
    } u;
    RefFloat *rd = Value_vp(*v);
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    char data[8];
    int i;

    u.d = rd->d;
    for (i = 0; i < 8; i++) {
        data[i] = (u.i >> ((7 - i) * 8)) & 0xFF;
    }
    if (!stream_write_data(w, data, 8)) {
        return FALSE;
    }

    return TRUE;
}
static int float_empty(Value *vret, Value *v, RefNode *node)
{
    RefFloat *rd = Value_vp(*v);
    *vret = bool_Value(rd->d == 0.0);
    return TRUE;
}

int float_hash(Value *vret, Value *v, RefNode *node)
{
    union {
        int32_t i[2];
        double d;
    } u;
    RefFloat *rd = Value_vp(*v);
    long hash;

    u.d = rd->d;
    hash = (u.i[0] ^ u.i[1]) & INT32_MAX;
    *vret = int32_Value(hash);

    return TRUE;
}
int float_eq(Value *vret, Value *v, RefNode *node)
{
    RefFloat *d1 = Value_vp(*v);
    RefFloat *d2 = Value_vp(v[1]);
    *vret = bool_Value(d1->d == d2->d);
    return TRUE;
}
int float_cmp(Value *vret, Value *v, RefNode *node)
{
    RefFloat *d1 = Value_vp(*v);
    RefFloat *d2 = Value_vp(v[1]);
    int cmp;

    if (d1->d < d2->d) {
        cmp = -1;
    } else if (d1->d > d2->d) {
        cmp = 1;
    } else {
        cmp = 0;
    }
    *vret = int32_Value(cmp);

    return TRUE;
}
static int float_negative(Value *vret, Value *v, RefNode *node)
{
    RefFloat *d1 = Value_vp(*v);
    RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));

    *vret = vp_Value(rd);
    rd->d = -d1->d;

    return TRUE;
}
static int float_addsubmul(Value *vret, Value *v, RefNode *node)
{
    RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
    RefFloat *d1 = Value_vp(*v);
    RefFloat *d2 = Value_vp(v[1]);

    *vret = vp_Value(rd);

    switch (FUNC_INT(node)) {
    case T_ADD:
        rd->d = d1->d + d2->d;
        break;
    case T_SUB:
        rd->d = d1->d - d2->d;
        break;
    case T_MUL:
        rd->d = d1->d * d2->d;
        break;
    }
    if (isnan(rd->d)) {
        throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
        return FALSE;
    }

    return TRUE;
}
static int float_div(Value *vret, Value *v, RefNode *node)
{
    RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
    RefFloat *d1 = Value_vp(*v);
    RefFloat *d2 = Value_vp(v[1]);

    *vret = vp_Value(rd);
    rd->d = d1->d / d2->d;

    if (isnan(rd->d)) {
        throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
        return FALSE;
    }

    return TRUE;
}
static int float_sign(Value *vret, Value *v, RefNode *node)
{
    double d = Value_float2(*v);
    if (d > 0.0) {
        *vret = int32_Value(1);
    } else if (d < 0.0) {
        *vret = int32_Value(-1);
    } else {
        *vret = int32_Value(0);
    }
    return TRUE;
}
static int float_is_negative(Value *vret, Value *v, RefNode *node)
{
    union {
        double d;
        uint64_t i;
    } u;
    u.d = Value_float2(*v);
    if ((u.i & 0x8000000000000000ULL) != 0) {
        *vret = VALUE_TRUE;
    } else {
        *vret = VALUE_FALSE;
    }
    return TRUE;
}
static int float_isinf(Value *vret, Value *v, RefNode *node)
{
    double d = Value_float2(*v);
    *vret = bool_Value(isinf(d));
    return TRUE;
}
static int float_toint(Value *vret, Value *v, RefNode *node)
{
    double d = Value_float2(*v);

    if (isinf(d)) {
        throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
        return FALSE;
    } else if (d <= (double)INT32_MAX && d >= -(double)INT32_MAX) {
        *vret = int32_Value((int32_t)d);
    } else {
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(mp);
        BigInt_init(&mp->bi);
        double_BigInt(&mp->bi, d);
    }
    return TRUE;
}
static int float_tofrac(Value *vret, Value *v, RefNode *node)
{
    double d = Value_float2(*v);
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));

    if (isinf(d)) {
        throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
        return FALSE;
    } else {
        *vret = vp_Value(md);
        BigInt_init(&md->bi[0]);
        BigInt_init(&md->bi[1]);
        double_BigRat(md->bi, d);
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

static int number_minmax(Value *vret, Value *v, RefNode *node)
{
    int is_min = FUNC_INT(node);
    Value result = Value_cp(v[1]);
    int i;
    int argc = fg->stk_top - v - 1;

    for (i = 2; i <= argc; i++) {
        int cmp = value_cmp_invoke(result, v[i], VALUE_NULL);
        if (cmp == VALUE_CMP_ERROR) {
            unref(result);
            return FALSE;
        }
        if ((is_min && cmp == VALUE_CMP_GT) || (!is_min && cmp == VALUE_CMP_LT)) {
            unref(result);
            result = Value_cp(v[i]);
        }
    }
    *vret = result;
    return TRUE;
}
static int number_clamp(Value *vret, Value *v, RefNode *node)
{
    Value result = Value_cp(v[1]);
    Value lower = Value_cp(v[2]);
    Value upper = Value_cp(v[3]);

    if (lower != VALUE_NULL) {
        int cmp = value_cmp_invoke(result, lower, VALUE_NULL);
        if (cmp == VALUE_CMP_ERROR) {
            goto ERROR_END;
        }
        if (cmp == VALUE_CMP_LT) {
            unref(result);
            result = Value_cp(lower);
        }
    }
    if (upper != VALUE_NULL) {
        int cmp = value_cmp_invoke(result, upper, VALUE_NULL);
        if (cmp == VALUE_CMP_ERROR) {
            goto ERROR_END;
        }
        if (cmp == VALUE_CMP_GT) {
            unref(result);
            result = Value_cp(upper);
        }
    }

    unref(lower);
    unref(upper);
    *vret = result;
    return TRUE;

ERROR_END:
    unref(result);
    unref(lower);
    unref(upper);
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static int char_new(Value *vret, Value *v, RefNode *node)
{
    int32_t n = 0;

    if (v + 1 < fg->stk_top) {
        Value v1 = v[1];
        RefNode *v1_type = Value_type(v1);
        if (v1_type == fs->cls_int) {
            if (Value_isref(v1)) {
                n = -1;
            }
            n = Value_integral(v1);
            if (n < 0 || n >= CODEPOINT_END || (n >= SURROGATE_U_BEGIN && n < SURROGATE_END)) {
                throw_errorf(fs->mod_lang, "ValueError", "Character range error (U+0000 - U+10FFFF)");
                return FALSE;
            }
        } else if (v1_type == fs->cls_char) {
            n = Value_integral(v1);
        } else if (v1_type == fs->cls_str) {
            RefStr *rs = Value_vp(v1);
            const char *p = rs->c;
            const char *end = p + rs->size;
            utf8_next(&p, end);
            if (rs->size == 0 || p != end) {
                throw_errorf(fs->mod_lang, "ValueError", "Argument string length is not 1");
                return FALSE;
            }
            n = utf8_codepoint_at(rs->c);
        }
    }

    *vret = integral_Value(fs->cls_char, n);
    return TRUE;
}
static int char_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint32_t n;

    if (!stream_read_uint32(r, &n)) {
        return FALSE;
    }
    if (n >= CODEPOINT_END || (n >= SURROGATE_U_BEGIN && n < SURROGATE_END)) {
        throw_errorf(fs->mod_lang, "ValueError", "Character range error (U+0000 - U+10FFFF)");
        return FALSE;
    }
    *vret = integral_Value(fs->cls_char, n);

    return TRUE;
}
static int char_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    int32_t ival = Value_integral(*v);
    if (!stream_write_uint32(w, ival)) {
        return FALSE;
    }
    return TRUE;
}
static int char_addsub(Value *vret, Value *v, RefNode *node)
{
    int32_t v0 = Value_integral(*v);
    int32_t n = -1;

    if (FUNC_INT(node)) {
        Value v1 = v[1];
        if (Value_isintegral(v1)) {
            int32_t n1 = Value_integral(v1);
            RefNode *v1_type = Value_type(v1);
            if (v1_type == fs->cls_int) {
                n = v0 - n1;
            } else if (v1_type == fs->cls_char) {
                *vret = integral_Value(fs->cls_int, v0 - n1);
                return TRUE;
            } else {
                throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_int, fs->cls_char, v1_type, 1);
                return TRUE;
            }
        }
    } else {
        if (Value_isint(v[1])) {
            int32_t v1 = Value_integral(v[1]);
            n = v0 + v1;
        }
    }
    if (n < 0 || n >= CODEPOINT_END || (n >= SURROGATE_U_BEGIN && n < SURROGATE_END)) {
        throw_errorf(fs->mod_lang, "ValueError", "Character range error (U+0000 - U+10FFFF)");
        return FALSE;
    }
    *vret = integral_Value(fs->cls_char, n);
    return TRUE;
}
static int char_inc(Value *vret, Value *v, RefNode *node)
{
    int32_t n = Value_integral(*v);
    int inc = FUNC_INT(node);
    n += inc;
    if (n < 0 || n >= CODEPOINT_END || (n >= SURROGATE_U_BEGIN && n < SURROGATE_END)) {
        throw_errorf(fs->mod_lang, "ValueError", "Character range error (U+0000 - U+10FFFF)");
        return FALSE;
    }
    *vret = integral_Value(fs->cls_char, n);
    return TRUE;
}
static int char_toint(Value *vret, Value *v, RefNode *node)
{
    int32_t n = Value_integral(*v);
    *vret = int32_Value(n);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

void define_lang_number_func(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "max", NODE_FUNC_N, 0);
    define_native_func_a(n, number_minmax, 1, -1, (void*)FALSE, NULL);

    n = define_identifier(m, m, "min", NODE_FUNC_N, 0);
    define_native_func_a(n, number_minmax, 1, -1, (void*)TRUE, NULL);

    n = define_identifier(m, m, "clamp", NODE_FUNC_N, 0);
    define_native_func_a(n, number_clamp, 3, 3, NULL, NULL, NULL, NULL);

    n = define_identifier(m, m, "b58encode", NODE_FUNC_N, 0);
    define_native_func_a(n, base58encode, 1, 2, NULL, fs->cls_int, fs->cls_str);

    n = define_identifier(m, m, "b58decode", NODE_FUNC_N, 0);
    define_native_func_a(n, base58decode, 1, 2, NULL, fs->cls_sequence, fs->cls_str);
}
void define_lang_number_class(RefNode *m)
{
    RefNode *n;
    RefNode *cls;
    RefStr *empty = intern("empty", -1);
    RefStr *toint = intern("to_int", -1);
    RefStr *tofloat = intern("to_float", -1);
    RefStr *tofrac = intern("to_frac", -1);

    // Number
    cls = fs->cls_number;
    extends_method(cls, fs->cls_obj);

    // Int
    cls = fs->cls_int;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, integer_new, 0, 1, NULL, NULL);
    n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    define_native_func_a(n, integer_parse, 1, 2, NULL, fs->cls_str, fs->cls_int);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, integer_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    define_native_func_a(n, integer_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, integer_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, integer_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, integer_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, integer_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_eq, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_cmp, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_addsub, 1, 1, (void*)FALSE, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_addsub, 1, 1, (void*)TRUE, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_mul, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_div, 1, 1, (void*)FALSE, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MOD], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_div, 1, 1, (void*)TRUE, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_PLUS], NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_this, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MINUS], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_negative, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_INC], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_inc, 0, 0, (void*) 1);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DEC], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_inc, 0, 0, (void*) -1);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_AND], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_logical, 1, 1, (void*)T_AND, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_OR], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_logical, 1, 1, (void*)T_OR, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_XOR], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_logical, 1, 1, (void*)T_XOR, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LSH], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_shift, 1, 1, (void*)FALSE, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_RSH], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_shift, 1, 1, (void*)TRUE, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_INV], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_inv, 0, 0, NULL);
    n = define_identifier(m, cls, "sign", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, integer_sign, 0, 0, NULL);
    n = define_identifier_p(m, cls, toint, NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_this, 0, 0, NULL);
    n = define_identifier_p(m, cls, tofloat, NODE_FUNC_N, 0);
    define_native_func_a(n, integer_tofloat, 0, 0, NULL);
    n = define_identifier_p(m, cls, tofrac, NODE_FUNC_N, 0);
    define_native_func_a(n, integer_tofrac, 0, 0, NULL);
    n = define_identifier(m, cls, "to_char", NODE_FUNC_N, 0);
    define_native_func_a(n, integer_tochar, 0, 0, NULL);
    extends_method(cls, fs->cls_number);

    // Frac
    cls = fs->cls_frac;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, frac_new, 0, 2, NULL, NULL, fs->cls_int);
    n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    define_native_func_a(n, frac_parse, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, frac_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    define_native_func_a(n, frac_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, frac_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, frac_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, frac_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, frac_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, frac_eq, 1, 1, NULL, fs->cls_frac);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, frac_cmp, 1, 1, NULL, fs->cls_frac);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_PLUS], NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_this, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MINUS], NODE_FUNC_N, 0);
    define_native_func_a(n, frac_negative, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, frac_addsub, 1, 1, (void*)FALSE, fs->cls_frac);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, frac_addsub, 1, 1, (void*)TRUE, fs->cls_frac);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    define_native_func_a(n, frac_mul, 1, 1, NULL, fs->cls_frac);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    define_native_func_a(n, frac_div, 1, 1, (void*)FALSE, fs->cls_frac);
    n = define_identifier(m, cls, "numerator", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, frac_get_part, 0, 0, (void*)0);
    n = define_identifier(m, cls, "denominator", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, frac_get_part, 0, 0, (void*)1);
    n = define_identifier(m, cls, "sign", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, frac_sign, 0, 0, NULL);
    n = define_identifier(m, cls, "round", NODE_FUNC_N, 0);
    define_native_func_a(n, frac_round, 1, 2, NULL, fs->cls_str, fs->cls_int);
    n = define_identifier_p(m, cls, toint, NODE_FUNC_N, 0);
    define_native_func_a(n, frac_toint, 0, 0, NULL);
    n = define_identifier_p(m, cls, tofloat, NODE_FUNC_N, 0);
    define_native_func_a(n, frac_tofloat, 0, 0, NULL);
    n = define_identifier_p(m, cls, tofrac, NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_this, 0, 0, NULL);
    extends_method(cls, fs->cls_number);

    // Float
    cls = fs->cls_float;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, float_new, 0, 1, NULL, NULL);
    n = define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    define_native_func_a(n, float_parse, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, float_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, float_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, float_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, float_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, float_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, float_eq, 1, 1, NULL, fs->cls_float);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, float_cmp, 1, 1, NULL, fs->cls_float);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_PLUS], NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_this, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MINUS], NODE_FUNC_N, 0);
    define_native_func_a(n, float_negative, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, float_addsubmul, 1, 1, (void*)T_ADD, fs->cls_float);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, float_addsubmul, 1, 1, (void*)T_SUB, fs->cls_float);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    define_native_func_a(n, float_addsubmul, 1, 1, (void*)T_MUL, fs->cls_float);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    define_native_func_a(n, float_div, 1, 1, NULL, fs->cls_float);
    n = define_identifier(m, cls, "isinf", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, float_isinf, 0, 0, NULL);
    n = define_identifier(m, cls, "sign", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, float_sign, 0, 0, NULL);
    n = define_identifier(m, cls, "is_negative", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, float_is_negative, 0, 0, NULL);
    n = define_identifier_p(m, cls, toint, NODE_FUNC_N, 0);
    define_native_func_a(n, float_toint, 0, 0, NULL);
    n = define_identifier_p(m, cls, tofloat, NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_this, 0, 0, NULL);
    n = define_identifier_p(m, cls, tofrac, NODE_FUNC_N, 0);
    define_native_func_a(n, float_tofrac, 0, 0, NULL);

    n = define_identifier(m, cls, "INF", NODE_CONST, 0);
    n->u.k.val = float_Value(fs->cls_float, 1.0 / 0.0);
    extends_method(cls, fs->cls_number);
    n = define_identifier(m, cls, "EPSILON", NODE_CONST, 0);
    n->u.k.val = float_Value(fs->cls_float, DBL_EPSILON);

    extends_method(cls, fs->cls_number);

    // Char
    cls = fs->cls_char;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, char_new, 0, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, char_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, integer_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, char_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, integer_hash, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, char_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_eq, 1, 1, NULL, fs->cls_char);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, integer_cmp, 1, 1, NULL, fs->cls_char);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    define_native_func_a(n, char_addsub, 1, 1, (void*)FALSE, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    define_native_func_a(n, char_addsub, 1, 1, (void*)TRUE, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_INC], NODE_FUNC_N, 0);
    define_native_func_a(n, char_inc, 0, 0, (void*) 1);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DEC], NODE_FUNC_N, 0);
    define_native_func_a(n, char_inc, 0, 0, (void*) -1);
    n = define_identifier_p(m, cls, toint, NODE_FUNC_N, 0);
    define_native_func_a(n, char_toint, 0, 0, NULL);

    extends_method(cls, fs->cls_obj);
}
