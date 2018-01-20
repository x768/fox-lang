#include "fox_vm.h"
#include "bigint.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stddef.h>


typedef struct NumberFormat {
    char sign;      // 符号 '+'=0以上の場合+を表示、'-'=0以上の場合スペース、'~'=負の数は補数表示 '\0'=表示しない
    char padding;   // パディング文字
    char other;     // 認識できない文字
    int upper;      // 11進数以上の場合、大文字を使う
    int group;      // 桁区切り
    int base;       // 基数
    int width;      // 整数部
    int width_f;    // 小数部
} NumberFormat;

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

/**
 * i >= 0
 * dstの後ろから詰める
 */
static char *ltostr(char *dst, int64_t i, int base, int upper)
{
    char al = (upper ? 'A' : 'a') - 10;
    int minus;

    *--dst = '\0';
    if (i < 0) {
        minus = TRUE;
        i = -i;
    } else if (i == 0) {
        *--dst = '0';
        return dst;
    } else {
        minus = FALSE;
    }

    while (i > 0) {
        int m = i % base;
        i /= base;
        if (m < 10) {
            *--dst = m + '0';
        } else {
            *--dst = m + al;
        }
    }
    if (minus) {
        *--dst = '-';
    }

    return dst;
}
/**
 * 2の補数
 * i < 0
 * dstの後ろから詰める
 */
static char *ltostr_complement(char *dst, int64_t i, int base, int upper)
{
    char al = (upper ? 'A' : 'a') - 10;
    int cmax = base - 1;

    i = -i - 1;
    *--dst = '\0';

    while (i > 0) {
        int m = cmax - i % base;
        i /= base;
        if (m < 10) {
            *--dst = m + '0';
        } else {
            *--dst = m + al;
        }
    }
    if (cmax < 10) {
        *--dst = cmax + '0';
    } else {
        *--dst = cmax + al;
    }
    *--dst = '~';

    return dst;
}
/**
 * 数値のフォーマット文字列解析
 */
static int parse_format_str(NumberFormat *nf, const char *fmt_p, int fmt_size)
{
    int i = 0;

    nf->sign = '\0';
    nf->padding = ' ';
    nf->other = '\0';
    nf->base = 10;
    nf->upper = FALSE;
    nf->group = FALSE;
    nf->width = 0;
    nf->width_f = -1;

    // フォーマット文字列解析
    // 5  -> "   12"
    // 05 -> "00012"
    if (i < fmt_size && (fmt_p[i] == '+' || fmt_p[i] == '-' || fmt_p[i] == '~')) {
        nf->sign = fmt_p[i];
        i++;
    }
    if (i < fmt_size) {
        char c = fmt_p[i];

        if (c == ',') {
            nf->group = TRUE;
            i++;
        } else {
            int width = 0;
            if (c == '0' || (!isalnum(c) && c != '.')) {
                if ((c & 0xFF) < ' ' || (c & 0xFF) >= 0x7F) {
                    throw_errorf(fs->mod_lang, "FormatError", "Invalid character (%c)", c);
                    return FALSE;
                }
                nf->padding = c;
                i++;
            }
            for ( ; i < fmt_size && isdigit(fmt_p[i]); i++) {
                width = width * 10 + (fmt_p[i] - '0');
                if (width > 32767) {
                    throw_errorf(fs->mod_lang, "FormatError", "Width number overflow (0 - 32767)");
                    return FALSE;
                }
            }
            nf->width = width;
        }
        if (i < fmt_size && fmt_p[i] == '.') {
            int width = 0;
            i++;
            for ( ; i < fmt_size && isdigit(fmt_p[i]); i++) {
                width = width * 10 + (fmt_p[i] - '0');
                if (width > 32767) {
                    throw_errorf(fs->mod_lang, "FormatError", "Width number overflow (0 - 32767)");
                    return FALSE;
                }
            }
            nf->width_f = width;
        }
    }
    if (i < fmt_size) {
        char c = fmt_p[i];
        nf->upper = isupper(c);

        switch (c) {
        case 'b': case 'B':
            nf->base = 2;
            break;
        case 'o': case 'O':
            nf->base = 8;
            break;
        case 'd': case 'D':
            break;
        case 'x': case 'X':
            nf->base = 16;
            break;
        case 'r': case 'R': {
            int base = 0;
            i++;
            if (i < fmt_size) {
                for ( ; i < fmt_size && isdigit(fmt_p[i]); i++) {
                    base = base * 10 + (fmt_p[i] - '0');
                    if (base > 36) {
                        throw_errorf(fs->mod_lang, "FormatError", "Invalid radix number(2 - 36)");
                        return FALSE;
                    }
                }
                if (base < 2) {
                    throw_errorf(fs->mod_lang, "FormatError", "Invalid radix number(2 - 36)");
                    return FALSE;
                }
                nf->base = base;
            } else {
                nf->other = c;
            }
            break;
        }
        default:
            nf->other = c;
            break;
        }
    }
    // 桁区切りは10進数しか使えない
    if (nf->group && nf->base != 10) {
        throw_errorf(fs->mod_lang, "FormatError", "',' appears but not decimal");
        return FALSE;
    }
    // 小数点以下桁数は、b o d x r に対して使えない
    if (nf->other == '\0' && nf->width_f > 0) {
        throw_errorf(fs->mod_lang, "FormatError", "Invalid use '.%d' for integer", nf->width_f);
        return FALSE;
    }
    // 2の補数のパディング
    if (nf->sign == '~' && nf->padding == '0') {
        int cmax = nf->base - 1;
        if (cmax < 10) {
            nf->padding = cmax + '0';
        } else {
            char al = (nf->upper ? 'A' : 'a') - 10;
            nf->padding = cmax + al;
        }
    }

    return TRUE;
}
/**
 * 数値をフォーマットしたときの長さを下回らない値を返します
 */
static int number_format_maxlength(const char *src, NumberFormat *nf, const LocaleData *loc)
{
    int len = 8;  // 符号
    int n, i;

    // 整数部の桁数
    for (i = 0; src[i] != '\0'; i++) {
        if (src[i] == '.') {
            break;
        }
    }
    n = i;
    src += n;
    if (*src == '.') {
        src++;
    }

    if (nf->group) {
        len += n + ((n - 1) / loc->group_n) * strlen(loc->group);
    } else {
        len += (n >= nf->width) ? n : nf->width;
    }
    // 小数部
    if (nf->width_f > 0) {
        len += strlen(loc->decimal) + nf->width_f;
    } else if (nf->width_f < 0 && *src != '\0') {
        len += strlen(loc->decimal) + strlen(src);
    }

    return len;
}
/**
 * 入力 12345.67
 * 出力 +12,345.6700
 */
static void number_format_str(char *dst, int *plen, const char *src, NumberFormat *nf, const LocaleData *loc)
{
    char *p = dst;
    int n, i;

    // 符号
    if (*src == '-' || *src == '~') {
        *p++ = *src;
        src++;
    } else if (nf->sign != '\0' && nf->sign != '~') {
        *p++ = nf->sign;
    }

    // 整数部の桁数
    for (i = 0; src[i] != '\0'; i++) {
        if (src[i] == '.') {
            break;
        }
    }
    n = i;

    // 整数部の変換を行う
    if (nf->group) {
        // 桁区切り
        const char *grp = loc->group;
        int grp_size = strlen(loc->group);
        int grp_n = loc->group_n;

        for (;;) {
            *p++ = *src++;
            n--;
            if (n == 0) {
                break;
            }
            if ((n % grp_n) == 0) {
                memcpy(p, grp, grp_size);
                p += grp_size;
            }
        }
    } else {
        // 桁数が不足している場合は、paddingを詰める
        if (nf->width > n) {
            memset(p, nf->padding, nf->width - n);
            p += nf->width - n;
        }
        memcpy(p, src, n);
        p += n;
        src += n;
    }
    if (*src == '.') {
        src++;
    }

    // 小数部
    if (nf->width_f > 0) {
        int w = nf->width_f;
        int end = FALSE;

        strcpy(p, loc->decimal);
        p += strlen(loc->decimal);

        for (i = 0; i < w; i++) {
            if (!end && src[i] == '\0') {
                end = TRUE;
            }

            if (end) {
                *p++ = '0';
            } else {
                *p++ = src[i];
            }
        }
    } else if (nf->width_f < 0 && *src != '\0') {
        const char *q;

        strcpy(p, loc->decimal);
        p += strlen(loc->decimal);

        // 末尾の0を削除
        for (q = src + strlen(src); q > src + 1; q--) { // 最低1つは残す
            if (q[-1] != '0') {
                break;
            }
        }

        memcpy(p, src, (q - src));
        p += (q - src);
    }

    *plen = (p - dst);
}
static Value BigInt_number_format(BigInt *bi, NumberFormat *nf, const LocaleData *loc)
{
    int len;
    char *ptr = malloc(BigInt_str_bufsize(bi, nf->base) + 2);
    RefStr *rs;
    BigInt_str(bi, nf->base, ptr, nf->upper);

    len = number_format_maxlength(ptr, nf, loc);
    rs = refstr_new_n(fs->cls_str, len);
    number_format_str(rs->c, &len, ptr, nf, loc);
    rs->c[len] = '\0';
    rs->size = len;

    return vp_Value(rs);
}

/**
 * 1000 -> 1.0K
 * 小数点以下を含む場合はdeimal=TRUE
 */
static Value number_to_si_unit(double val, NumberFormat *nf, const LocaleData *loc, int decimal)
{
    char buf[34];
    char *ptr = buf;
    double factor;

    if (nf->width_f < 0) {
        nf->width_f = 1;
    } else if (nf->width_f > 12) {
        nf->width_f = 12;
    }

    if (val < 0.0) {
        val = -val;
        *ptr++ = '-';
    }
    if (nf->sign != '\0') {
        *ptr++ = nf->sign;
    }

    if (nf->other == 'S') {
        factor = 1024.0;
    } else {
        factor = 1000.0;
    }
    if (isnan(val) || isinf(val)) {
        strcpy(buf, "(overflow)");
        goto FINISH;
    } else if (val < 1.0 && val != 0.0) {
        const char *post = " munpfazy";
        while (val < 1.0) {
            val *= factor;
            post++;
            if (*post == '\0') {
                strcpy(buf, "0");
                goto FINISH;
            }
        }
        if (nf->width_f == 0) {
            sprintf(ptr, "%f %c", val, *post);
        } else {
            sprintf(ptr, "%.*f %c", nf->width_f, val, *post);
        }
    } else if (val < factor) {
        if (decimal) {
            if (nf->width_f == 0) {
                sprintf(ptr, "%f ", val);
            } else {
                sprintf(ptr, "%.*f ", nf->width_f, val);
            }
        } else {
            sprintf(ptr, "%d ", (int)val);
        }
    } else {
        const char *post = " kMGTPEZY";
        char pch;
        while (val >= factor) {
            val /= factor;
            post++;
            if (*post == '\0') {
                strcpy(buf, "(overflow)");
                goto FINISH;
            }
        }
        pch = *post;
        if (nf->other == 'S' && pch == 'k') {
            pch = 'K';
        }
        if (nf->width_f == 0) {
            sprintf(ptr, "%f %c", val, pch);
        } else {
            if (nf->other == 'S') {
                sprintf(ptr, "%.*f %ci", nf->width_f, val, pch);
            } else {
                sprintf(ptr, "%.*f %c", nf->width_f, val, pch);
            }
        }
    }
FINISH:
    // 小数点をRefLocaleに従って置換
    {
        int j;
        StrBuf sb;

        StrBuf_init_refstr(&sb, 0);
        for (j = 0; buf[j] != '\0'; j++) {
            if (buf[j] == '.') {
                StrBuf_add(&sb, buf, j);
                StrBuf_add(&sb, loc->decimal, -1);
                StrBuf_add(&sb, &buf[j + 1], -1);
                break;
            }
        }
        if (buf[j] == '\0') {
            StrBuf_add(&sb, buf, j);
        }
        return StrBuf_str_Value(&sb, fs->cls_str);
    }
}

/**
 * 入力:v (Int)
 * 出力:v (Str)
 */
static int integer_tostr_sub(Value *vret, Value v, RefStr *fmt, const LocaleData *loc)
{
    NumberFormat nf;

    if (!parse_format_str(&nf, fmt->c, fmt->size)) {
        return FALSE;
    }

    switch (nf.other) {
    case '\0': case 'f': {
        char *p_ptr = NULL;
        char *ptr = NULL;
        RefStr *rs;
        int len;
        char c_buf[40];

        if (nf.other == 'f') {
            nf.base = 10;
            nf.upper = FALSE;
        }

        // X進数に変換する
        if (Value_isint(v)) {
            int32_t val = Value_integral(v);
            if (nf.sign == '~' && val < 0) {
                ptr = ltostr_complement(c_buf + sizeof(c_buf) - 4, val, nf.base, nf.upper);
            } else {
                ptr = ltostr(c_buf + sizeof(c_buf) - 4, val, nf.base, nf.upper);
            }
        } else {
            RefInt *mp = Value_vp(v);
            p_ptr = malloc(BigInt_str_bufsize(&mp->bi, nf.base) + 8);
            ptr = p_ptr;
            BigInt_str(&mp->bi, nf.base, ptr, nf.upper);
        }
        if (nf.other == 'f') {
            strcat(ptr, ".0");
        }

        len = number_format_maxlength(ptr, &nf, loc);
        rs = refstr_new_n(fs->cls_str, len);
        *vret = vp_Value(rs);
        number_format_str(rs->c, &len, ptr, &nf, loc);
        rs->c[len] = '\0';
        rs->size = len;

        free(p_ptr);
        break;
    }
    case 's': case 'S': {
        // SI接頭語
        double val;
        if (Value_isint(v)) {
            val = (double)Value_integral(v);
        } else {
            RefInt *mp = Value_vp(v);
            val = BigInt_double(&mp->bi);
        }
        *vret = number_to_si_unit(val, &nf, loc, FALSE);
        break;
    }
    default:
        throw_errorf(fs->mod_lang, "FormatError", "Unknown format string %c", nf.other);
        return FALSE;
    }
    return TRUE;
}
static int integer_tostr(Value *vret, Value *v, RefNode *node)
{
    LocaleData *loc;

    if (fg->stk_top > v + 1) {
        if (fg->stk_top > v + 2) {
            loc = Value_locale_data(v[2]);
        } else {
            loc = fv->loc_neutral;
        }
        if (!integer_tostr_sub(vret, *v, Value_vp(v[1]), loc)) {
            return FALSE;
        }
    } else {
        // 単純に10進数に変換するだけ
        if (Value_isint(*v)) {
            char c_buf[32];
            sprintf(c_buf, "%d", Value_integral(*v));
            *vret = cstr_Value(fs->cls_str, c_buf, -1);
        } else {
            RefInt *mp = Value_vp(*v);
            char *c_buf = malloc(BigInt_str_bufsize(&mp->bi, 10) + 2);
            BigInt_str(&mp->bi, 10, c_buf, FALSE);
            *vret = cstr_Value(fs->cls_str, c_buf, -1);
            free(c_buf);
        }
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

    *vret = vp_Value(md);

    BigInt_init(&md->bi[0]);
    BigInt_init(&md->bi[1]);
    if (!cstr_BigRat(md->bi, rs->c, &end) || *end != '\0') {
        // 読み込みエラー時は例外
        throw_errorf(fs->mod_lang, "ParseError", "Invalid rational string");
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

static int frac_tostr(Value *vret, Value *v, RefNode *node)
{
    RefFrac *md = Value_vp(*v);
    NumberFormat nf;
    LocaleData *loc = fv->loc_neutral;

    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);

        if (!parse_format_str(&nf, fmt->c, fmt->size)) {
            return FALSE;
        }
        if (nf.base != 10) {
            throw_errorf(fs->mod_lang, "FormatError", "Frac format is decimal only");
            return FALSE;
        }
        if (nf.sign == '~') {
            throw_errorf(fs->mod_lang, "FormatError", "Format ~ is only for Int");
            return FALSE;
        }

        if (fg->stk_top > v + 2) {
            loc = Value_locale_data(v[2]);
        }
    } else {
        nf.sign = '\0';
        nf.padding = '\0';
        nf.other = 'f';
        nf.base = 10;
        nf.upper = FALSE;
        nf.group = FALSE;
        nf.width = 0;
        nf.width_f = -1;
    }

    switch (nf.other) {
    case 'f':
    case 'p': {
        // 整数部
        char *c_buf;
        int len;
        BigInt mi, rem;
        int ellip = FALSE;
        BigInt_init(&mi);
        BigInt_init(&rem);
        BigInt_divmod(&mi, &rem, &md->bi[0], &md->bi[1]);
        if (rem.sign < 0) {
            rem.sign = 1;
        }
        // 小数部
        if (nf.width_f == 0) {
            // 整数部のみ出力
            c_buf = malloc(BigInt_str_bufsize(&mi, 10));
            BigInt_str(&mi, 10, c_buf, FALSE);
        } else {
            int recr[2];
            int n_ex;
            BigInt ex;

            if (nf.width_f > 0) {
                n_ex = nf.width_f;
            } else if (rem.sign == 0) {
                n_ex = 0;
            } else if (BigInt_get_recurrence(&md->bi[1], recr, 64)) {
                // 循環小数
                if (recr[1] - recr[0] > 16) {
                    n_ex = recr[1];
                } else {
                    n_ex = recr[0] + 15;
                }
                ellip = TRUE;
            } else {
                // 循環しない
                n_ex = recr[0];
            }
            BigInt_init(&ex);
            int64_BigInt(&ex, 10);

            BigInt_pow(&ex, n_ex);     // ex = ex ** width_f
            BigInt_mul(&rem, &rem, &ex);

            BigInt_divmod(&rem, NULL, &rem, &md->bi[1]);

            c_buf = BigRat_tostr_sub(md->bi[0].sign, &mi, &rem, n_ex);
            if (n_ex > 0) {
                BigInt_close(&ex);
            }
        }
        {
            RefStr *rs = refstr_new_n(fs->cls_str, number_format_maxlength(c_buf, &nf, loc));
            *vret = vp_Value(rs);
            number_format_str(rs->c, &len, c_buf, &nf, loc);
            if (ellip) {
                strcpy(rs->c + len, "...");
                rs->size = len + 3;
            } else {
                rs->c[len] = '\0';
                rs->size = len;
            }
        }

        free(c_buf);
        BigInt_close(&mi);
        BigInt_close(&rem);
        break;
    }
    case 'r': {
        // (numerator/denominator)
        int len;
        RefStr *rs = refstr_new_n(fs->cls_str, BigInt_str_bufsize(&md->bi[0], 10) + BigInt_str_bufsize(&md->bi[1], 10) + 5);
        *vret = vp_Value(rs);

        strcpy(rs->c, "(");
        BigInt_str(&md->bi[0], 10, rs->c + 1, FALSE);

        if (rs->c[1] == '-') {
            rs->c[0] = '-';
            rs->c[1] = '(';
        }

        len = strlen(rs->c);
        strcpy(rs->c + len, "/");
        BigInt_str(&md->bi[1], 10, rs->c + len + 1, FALSE);
        len = strlen(rs->c);
        strcpy(rs->c + len, ")");
        rs->size = len + 1;
        break;
    }
    case 's': case 'S': {
        // SI接頭語
        double d1 = BigInt_double(&md->bi[0]);
        double d2 = BigInt_double(&md->bi[1]);

        double val = d1 / d2;
        *vret = number_to_si_unit(val, &nf, loc, TRUE);
        break;
    }
    case '\0': {
        BigInt mp, rem;

        BigInt_init(&mp);
        BigInt_init(&rem);
        BigInt_divmod(&mp, &rem, &md->bi[0], &md->bi[1]);

        *vret = BigInt_number_format(&mp, &nf, loc);

        BigInt_close(&mp);
        BigInt_close(&rem);
        break;
    }
    default:
        throw_errorf(fs->mod_lang, "FormatError", "Unknown format string %c", nf.other);
        return FALSE;
    }

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

    *vret = vp_Value(rd);
    errno = 0;
    rd->d = strtod(rs->c, &end);
    if (errno != 0) {
        throw_errorf(fs->mod_lang, "ValueError", "%s", strerror(errno));
        return FALSE;
    }
    if (*end != '\0') {
        // 読み込みエラー時は例外
        throw_errorf(fs->mod_lang, "ParseError", "Invalid float string");
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

// src : -#.#######
static int adjust_float_str(char *dst, const char *src, int ex)
{
    if (*src == '-') {
        *dst++ = *src++;
    }

    if (ex < 0) {
        // 小さくする方向に桁をずらす
        int n = -ex - 1;
        *dst++ = '0';
        *dst++ = '.';
        memset(dst, '0', n);
        dst += n;

        *dst++ = *src++;
        if (*src == '.')      // 小数点を飛ばす
            src++;
        strcpy(dst, src);
    } else if (ex > 0) {
        // 大きくする方向に桁をずらす
        ex++;
        for (; *src != '\0'; src++) {
            int ch = *src;
            if (ch != '.') {
                *dst++ = ch;
                ex--;
                if (ex == 0 && src[1] != '\0') {
                    *dst++ = '.';
                }
            }
        }
        // 残りの0を追加
        if (ex > 0) {
            memset(dst, '0', ex);
            dst += ex;
        }
        *dst = '\0';
    } else {
        // そのまま
        strcpy(dst, src);
    }
    return 0;
}
static int float_tostr(Value *vret, Value *v, RefNode *node)
{
    RefFloat *rd = Value_vp(*v);
    LocaleData *loc = fv->loc_neutral;
    NumberFormat nf;
    char c_buf[32];
    int i, ex = 0;

    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);

        if (!parse_format_str(&nf, fmt->c, fmt->size)) {
            return FALSE;
        }
        if (nf.base != 10) {
            throw_errorf(fs->mod_lang, "FormatError", "Float format is decimal only");
            return FALSE;
        }
        if (nf.sign == '~') {
            throw_errorf(fs->mod_lang, "FormatError", "Format ~ is only for Int");
            return FALSE;
        }

        if (fg->stk_top > v + 2) {
            loc = Value_locale_data(v[2]);
        }
    } else {
        nf.sign = '\0';
        nf.padding = '\0';
        nf.other = 'g';
        nf.base = 10;
        nf.upper = FALSE;
        nf.group = FALSE;
        nf.width = 0;
        nf.width_f = -1;
    }

    if (isinf(rd->d)) {
        if (rd->d > 0) {
            if (nf.sign == '+') {
                *vret = cstr_Value(fs->cls_str, "+Inf", -1);
            } else {
                *vret = cstr_Value(fs->cls_str, "Inf", -1);
            }
        } else {
            *vret = cstr_Value(fs->cls_str, "-Inf", -1);
        }
        return TRUE;
    }

    if (nf.other == 'a' || nf.other == 'A') {
        sprintf(c_buf, nf.other == 'a' ? "%a" : "%A", rd->d);
        *vret = cstr_Value(fs->cls_str, c_buf, -1);
        return TRUE;
    }
    // 精度は16桁
    if (nf.width_f > 15) {
        sprintf(c_buf, "%.*e", nf.width_f, rd->d);
    } else {
        sprintf(c_buf, "%.15e", rd->d);
    }
    // 指数部と仮数部に分ける
    for (i = 0; c_buf[i] != '\0'; i++) {
        if (c_buf[i] == 'e') {
            c_buf[i++] = '\0';
            ex = strtol(&c_buf[i], NULL, 10);
        }
    }
    {
        // 末尾の0を除去
        char *p = c_buf + strlen(c_buf);
        while (p > c_buf) {
            if (p[-1] != '0') {
                if (p[-1] == '.') {
                    p--;
                }
                *p = '\0';
                break;
            }
            p--;
        }
    }

    // Plain formatと指数表記をいい具合に切り替える
    if (nf.other == 'g' || nf.other == 'G') {
        if ((nf.width_f == -1 && ex < -4) || (nf.width_f != -1 && ex <= -nf.width_f)) {
            nf.other = 'E' + (nf.other - 'G');
        } else if ((nf.width == 0 && ex > 10) || (nf.width != 0 && ex >= nf.width)) {
            nf.other = 'E' + (nf.other - 'G');  // 大文字小文字をnf.otherと同じにする
        } else {
            nf.other = 'F' + (nf.other - 'G');
        }
    }

    switch (nf.other) {
    case 'f': {
        RefStr *rs;
        int len;
        // 小数点をずらす
        char *c_tmp = malloc((ex < 0 ? -ex : ex) + 32);
        adjust_float_str(c_tmp, c_buf, ex);

        len = number_format_maxlength(c_tmp, &nf, loc);
        rs = refstr_new_n(fs->cls_str, len);
        *vret = vp_Value(rs);
        number_format_str(rs->c, &len, c_tmp, &nf, loc);
        rs->c[len] = '\0';
        rs->size = len;
        free(c_tmp);

        break;
    }
    case 'e': case 'E': {
        // フォーマットを調整
        int len = number_format_maxlength(c_buf, &nf, loc);
        RefStr *rs = refstr_new_n(fs->cls_str, len + 8);
        *vret = vp_Value(rs);
        number_format_str(rs->c, &len, c_buf, &nf, loc);
        sprintf(rs->c + len, "%c%+d", nf.other, ex);
        rs->size = strlen(rs->c);
        break;
    }
    case 's': case 'S':
        // SI接頭語
        *vret = number_to_si_unit(rd->d, &nf, loc, TRUE);
        break;
    case '\0': {
        BigInt mp;

        BigInt_init(&mp);
        double_BigInt(&mp, rd->d);
        *vret = BigInt_number_format(&mp, &nf, loc);
        BigInt_close(&mp);
        break;
    }
    default:
        throw_errorf(fs->mod_lang, "FormatError", "Unknown format string %c", nf.other);
        return FALSE;
    }

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
        if (isnan(rd->d)) {
            throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
            return FALSE;
        }
        break;
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

////////////////////////////////////////////////////////////////////////////////

static int valid_base58_fmt(const char *fmt)
{
    int mode[3] = {0, 0, 0};
    int i;

    for (i = 0; fmt[i] != '\0'; i++) {
        int idx = -1;
        switch (fmt[i]) {
        case '0':
            idx = 0;
            break;
        case 'A':
            idx = 1;
            break;
        case 'a':
            idx = 2;
            break;
        }
        if (idx == -1) {
            goto ERROR_END;
        }
        if (mode[idx]) {
            goto ERROR_END;
        }
        mode[idx] = TRUE;
    }
    if (i != 3) {
        goto ERROR_END;
    }
    return TRUE;

ERROR_END:
    throw_errorf(fs->mod_lang, "FormatError", "Illigal base58 option");
    return FALSE;
}
static void init_base58_table(char *dst, const char *fmt)
{
    int i;

    for (i = 0; i < 3; i++) {
        switch (fmt[i]) {
        case '0':
            strcpy(dst, "123456789");
            break;
        case 'A':
            strcpy(dst, "ABCDEFGHJKLMNPQRSTUVWXYZ");
            break;
        case 'a':
            strcpy(dst, "abcdefghijkmnopqrstuvwxyz");
            break;
        }
        dst += strlen(dst);
    }
}
static void reverse_cstr(char *p, int size)
{
    int i;
    int n = size / 2;

    for (i = 0; i < n; i++) {
        int j = size - i - 1;
        int tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
    }
}
static int base58encode(Value *vret, Value *v, RefNode *node)
{
    char base58[60];
    BigInt mp;
    BigInt *mpp;
    StrBuf buf;
    int offset = offsetof(RefStr, c);

    if (fg->stk_top > v + 2) {
        RefStr *fmt = Value_vp(v[2]);
        if (!valid_base58_fmt(fmt->c)) {
            return FALSE;
        }
        init_base58_table(base58, fmt->c);
    } else {
        init_base58_table(base58, "0aA");
    }

    if (Value_isref(v[1])) {
        RefInt *ri = Value_vp(v[1]);
        mpp = &ri->bi;
        if (mpp->sign < 0) {
            throw_errorf(fs->mod_lang, "ValueError", "Negative values not allowed here");
            return FALSE;
        }
    } else {
        int32_t i = Value_integral(v[1]);
        if (i < 0) {
            throw_errorf(fs->mod_lang, "ValueError", "Negative values not allowed here");
            return FALSE;
        }
        BigInt_init(&mp);
        int64_BigInt(&mp, i);
        mpp = &mp;
    }

    StrBuf_init_refstr(&buf, 0);
    for (;;) {
        uint16_t r;
        BigInt_divmod_sd(mpp, 58, &r);
        StrBuf_add_c(&buf, base58[r]);

        if (mpp->sign == 0) {
            break;
        }
    }
    if (mpp == &mp) {
        BigInt_close(&mp);
    }
    reverse_cstr(buf.p + offset, buf.size - offset);
    *vret = StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}

static void init_base58_dtable(char *dst, const char *fmt)
{
    int i, j;
    int n = 0;

    memset(dst, 0x7F, 128);

    for (i = 0; i < 3; i++) {
        switch (fmt[i]) {
        case '0':
            for (j = '1'; j <= '9'; j++) {
                dst[j] = n;
                n++;
            }
            break;
        case 'A':
            for (j = 'A'; j <= 'Z'; j++) {
                if (j == 'I' || j == 'O') {
                    j++;
                }
                dst[j] = n;
                n++;
            }
            break;
        case 'a':
            for (j = 'a'; j <= 'z'; j++) {
                if (j == 'l') {
                    j++;
                }
                dst[j] = n;
                n++;
            }
            break;
        }
    }
}
static int base58decode(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    RefInt *ri;
    char base58[128];
    int i;

    if (fg->stk_top > v + 2) {
        RefStr *fmt = Value_vp(v[2]);
        if (!valid_base58_fmt(fmt->c)) {
            return FALSE;
        }
        init_base58_dtable(base58, fmt->c);
    } else {
        init_base58_dtable(base58, "0aA");
    }

    ri = buf_new(fs->cls_int, sizeof(RefInt));
    BigInt_init(&ri->bi);
    for (i = 0; i < rs->size; i++) {
        uint8_t c = rs->c[i];
        int n;
        if (c >= 0x80) {
            goto ERROR_END;
        }
        n = base58[c];
        if (n >= 58) {
            goto ERROR_END;
        }
        BigInt_mul_sd(&ri->bi, 58);
        BigInt_add_d(&ri->bi, n);
    }
    *vret = vp_Value(ri);
    fix_bigint(vret, &ri->bi);

    return TRUE;

ERROR_END:
    throw_errorf(fs->mod_lang, "ValueError", "Illigal character");
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
static int char_tostr(Value *vret, Value *v, RefNode *node)
{
    int code = Value_integral(*v);

    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);
        char tmp[8];
        if (str_eq(fmt->c, fmt->size, "u", 1)) {
            sprintf(tmp, "U+%04X", code);
            *vret = cstr_Value(fs->cls_str, tmp, -1);
            return TRUE;
        } else if (str_eq(fmt->c, fmt->size, "x", 1)) {
            sprintf(tmp, "\\x{%X}", code);
            *vret = cstr_Value(fs->cls_str, tmp, -1);
            return TRUE;
        } else if (str_eq(fmt->c, fmt->size, "xml", 3)) {
            if (code >= ' ' && code < 0x80) {
                switch (code) {
                case '<':
                    strcpy(tmp, "&lt;");
                    break;
                case '>':
                    strcpy(tmp, "&gt;");
                    break;
                case '&':
                    strcpy(tmp, "&amp;");
                    break;
                case '"':
                    strcpy(tmp, "&quot;");
                    break;
                default:
                    tmp[0] = code;
                    tmp[1] = '\0';
                    break;
                }
            } else {
                sprintf(tmp, "&#%d;", code);
            }
            *vret = cstr_Value(fs->cls_str, tmp, -1);
            return TRUE;
        } else if (fmt->size > 0) {
            throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", Str_new(fmt->c, fmt->size));
            return FALSE;
        }
    }
    {
        char tmp[8];
        char *ptmp = tmp;
        if (!str_add_codepoint(&ptmp, code, NULL)) {
            return FALSE;
        }
        *vret = cstr_Value(fs->cls_str, tmp, ptmp - tmp);
    }
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
    n = define_identifier_p(m, cls, toint, NODE_FUNC_N, 0);
    define_native_func_a(n, char_toint, 0, 0, NULL);

    extends_method(cls, fs->cls_obj);
}
