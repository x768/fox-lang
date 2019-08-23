#define DEFINE_GLOBALS
#include "fox_math.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>


typedef struct {
    const char *name;
    double (*real)(double);
} MathFunc1;

typedef struct {
    const char *name;
    double (*real)(double);
    int (*complex)(RefComplex *r, const RefComplex *v1);
} MathCFunc1;

int math_native_return_this(Value *vret, Value *v, RefNode *node)
{
    *vret = fs->Value_cp(*v);
    return TRUE;
}
double bytes_to_double(const uint8_t *data)
{
    union {
        uint64_t i;
        double d;
    } u;
    int i;

    u.i = 0ULL;
    for (i = 0; i < 8; i++) {
        u.i |= ((uint64_t)data[i]) << ((7 - i) * 8);
    }
    return u.d;
}
void double_to_bytes(uint8_t *data, double val)
{
    union {
        uint64_t i;
        double d;
    } u;
    int i;

    u.d = val;
    for (i = 0; i < 8; i++) {
        data[i] = u.i >> ((7 - i) * 8) & 0xFF;
    }
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
            if (mp->bi.sign < 0) {
                RefInt *mp2 = fs->buf_new(fs->cls_int, sizeof(RefInt));
                *vret = vp_Value(mp2);
                fs->BigInt_init(&mp2->bi);
                fs->BigInt_copy(&mp2->bi, &mp->bi);
                mp2->bi.sign = 1;
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
        *vret = fs->float_Value(fs->cls_float, dval);
    } else if (v_type == fs->cls_frac) {
        // Frac
        RefFrac *md = Value_vp(v1);

        if (md->bi[0].sign < 0) {
            RefFrac *md2 = fs->buf_new(fs->cls_frac, sizeof(RefFrac));
            *vret = vp_Value(md2);
            fs->BigInt_init(&md2->bi[0]);
            fs->BigInt_init(&md2->bi[1]);
            fs->BigInt_copy(&md2->bi[0], &md->bi[0]);
            fs->BigInt_copy(&md2->bi[1], &md->bi[1]);
            md2->bi[0].sign = 1;
        } else {
            // move 参照カウンタは変わらない
            *vret = v1;
            v[1] = VALUE_NULL;
        }
    } else if (v_type == cls_complex) {
        RefComplex *rc = Value_vp(v1);
        double d = sqrt(rc->re * rc->re + rc->im * rc->im);
        *vret = fs->float_Value(fs->cls_float, d);
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_number, cls_complex, v_type, 1);
        return FALSE;
    }

    return TRUE;
}
static int math_pow_int(Value *vret, Value v, int n)
{
    if (n == 0) {
        *vret = int32_Value(1);
    } else if (n == 1) {
        *vret = fs->Value_cp(v);
    } else {
        BigInt mp1;
        int result;
        RefInt *ri = fs->buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(ri);

        if (Value_isref(v)) {
            RefInt *ri2 = Value_vp(v);
            mp1 = ri2->bi;
        } else {
            fs->BigInt_init(&mp1);
            fs->int64_BigInt(&mp1, Value_integral(v));
        }
        
        fs->BigInt_init(&ri->bi);
        fs->BigInt_copy(&ri->bi, &mp1);
        result = fs->BigInt_pow(&ri->bi, n);

        if (!Value_isref(v)) {
            fs->BigInt_close(&mp1);
        }
        if (!result) {
            fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }
        fs->fix_bigint(vret, &ri->bi);
    }
    return TRUE;
}
static int math_pow_frac(Value *vret, Value v, int n)
{
    if (n == 1) {
        *vret = fs->Value_cp(v);
    } else {
        RefFrac *rf1 = Value_vp(v);
        RefFrac *rf = fs->buf_new(fs->cls_frac, sizeof(RefFrac));
        *vret = vp_Value(rf);

        fs->BigInt_init(&rf->bi[0]);
        fs->BigInt_init(&rf->bi[1]);

        if (n == 0) {
            fs->int64_BigInt(&rf->bi[0], 1);
            fs->int64_BigInt(&rf->bi[1], 1);
        } else if (n == -1) {
            fs->BigInt_copy(&rf->bi[1], &rf1->bi[0]);
            fs->BigInt_copy(&rf->bi[0], &rf1->bi[1]);
        } else if (n < 0) {
            if (rf1->bi[0].sign == 0) {
                fs->throw_errorf(fs->mod_lang, "ZeroDivisionError", "pow(0d, -n)");
                return FALSE;
            }
            fs->BigInt_copy(&rf->bi[0], &rf1->bi[1]);
            if (!fs->BigInt_pow(&rf->bi[0], -n)) {
                goto MEMORY_ERROR;
            }
            fs->BigInt_copy(&rf->bi[1], &rf1->bi[0]);
            if (!fs->BigInt_pow(&rf->bi[1], -n)) {
                goto MEMORY_ERROR;
            }
        } else {
            fs->BigInt_copy(&rf->bi[0], &rf1->bi[0]);
            if (!fs->BigInt_pow(&rf->bi[0], n)) {
                goto MEMORY_ERROR;
            }
            fs->BigInt_copy(&rf->bi[1], &rf1->bi[1]);
            if (!fs->BigInt_pow(&rf->bi[1], n)) {
                goto MEMORY_ERROR;
            }
        }
    }
    return TRUE;
MEMORY_ERROR:
    fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
    return FALSE;
}
/**
 * pow(x, y) => x ** y
 */
static int math_pow(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    Value v2 = v[2];
    RefNode *v1_type = fs->Value_type(v1);
    RefNode *v2_type = fs->Value_type(v2);

    if (v1_type == fs->cls_int) {
        if (v2_type != fs->cls_int) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_int, v2_type, 2);
            return FALSE;
        } else {
            int i2 = Value_integral(v2);
            if (Value_isref(v2) || i2 < 0 || i2 > 32767) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal argument#2 value (0 - 32767)");
                return FALSE;
            }
            if (!math_pow_int(vret, v1, i2)) {
                return FALSE;
            }
        }
    } else if (v1_type == fs->cls_float) {
        if (v2_type != fs->cls_float) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_float, v2_type, 2);
            return FALSE;
        } else {
            double d1 = Value_float2(v1);
            double d2 = Value_float2(v2);
            double d = pow(d1, d2);
            if (isnan(d)) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal argument value");
                return FALSE;
            }
            *vret = fs->float_Value(fs->cls_float, d);
        }
    } else if (v1_type == fs->cls_frac) {
        if (v2_type != fs->cls_int) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_int, v2_type, 2);
            return FALSE;
        } else {
            int i2 = Value_integral(v2);
            if (Value_isref(v2) || i2 < -32767 || i2 > 32767) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal argument#2 value (-32767 - 32767)");
                return FALSE;
            }
            if (!math_pow_frac(vret, v1, i2)) {
                return FALSE;
            }
        }
    } else if (v1_type == cls_complex) {
        if (v2_type != cls_complex) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, cls_complex, v2_type, 2);
            return FALSE;
        } else {
            RefComplex *c1 = Value_vp(v1);
            RefComplex *c2 = Value_vp(v2);
            RefComplex *c = fs->buf_new(cls_complex, sizeof(RefComplex));
            *vret = vp_Value(c);

            if (c2->im == 0.0) {
                double re = sqrt(c1->re * c1->re + c1->im * c1->im);
                double re_2 = pow(re, c2->re);
                double arg = atan2(c1->im, c1->re) * c2->re;
                c->re = re_2 * cos(arg);
                c->im = re_2 * sin(arg);
            } else {
                double re1 = log(sqrt(c1->re * c1->re + c1->im * c1->im));
                double im1 = atan2(c1->im, c1->re);
                double re2 = exp(re1 * c2->re - im1 * c2->im);
                double im2 = im1 * c2->re + re1 * c2->im;
                c->re = re2 * cos(im2);
                c->im = re2 * sin(im2);
            }
        }
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Int, Float, Frac or Complex required but %n (argument #1)", v1);
        return FALSE;
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
        BigInt m, n;
        RefInt *gcd;

        fs->BigInt_init(&m);
        fs->BigInt_init(&n);
        if (Value_isref(v1)) {
            RefInt *r1 = Value_vp(v1);
            m = r1->bi;
        } else {
            fs->int64_BigInt(&m, Value_integral(v1));
        }
        if (Value_isref(v2)) {
            RefInt *r2 = Value_vp(v2);
            n = r2->bi;
        } else {
            fs->int64_BigInt(&n, Value_integral(v2));
        }

        if (m.sign < 0) {
            m.sign = 1;
        } else if (m.sign == 0) {
            goto ERROR_END;
        }
        if (n.sign < 0) {
            n.sign = 1;
        } else if (n.sign == 0) {
            goto ERROR_END;
        }

        gcd = fs->buf_new(fs->cls_int, sizeof(RefInt));
        *vret = vp_Value(gcd);
        fs->BigInt_init(&gcd->bi);
        fs->BigInt_gcd(&gcd->bi, &m, &n);
        fs->fix_bigint(vret, &gcd->bi);

        if (!Value_isref(v1)) {
            fs->BigInt_close(&m);
        }
        if (!Value_isref(v2)) {
            fs->BigInt_close(&n);
        }
    } else {
        int32_t m = Value_integral(v1);
        int32_t n = Value_integral(v2);

        if (m < 0) {
            m = -m;
        } else if (m == 0) {
            goto ERROR_END;
        }
        if (n < 0) {
            n = -n;
        } else if (n == 0) {
            goto ERROR_END;
        }
        // TODO:アルゴリズム修正
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

////////////////////////////////////////////////////////////////////////////////////////

static int complex_isvalid(const RefComplex *rc)
{
    if (isnan(rc->re) || isnan(rc->im)) {
        fs->throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
        return FALSE;
    }
    return TRUE;
}

static int complex_new(Value *vret, Value *v, RefNode *node)
{
    RefComplex *rc = fs->buf_new(cls_complex, sizeof(RefComplex));
    *vret = vp_Value(rc);

    if (fg->stk_top > v + 2) {
        int i;
        for (i = 1; i <= 2; i++) {
            const RefNode *v_type = fs->Value_type(v[i]);
            if (v_type != fs->cls_float) {
                fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_float, v_type, i);
                return FALSE;
            }
        }
        rc->re = Value_float2(v[1]);
        rc->im = Value_float2(v[2]);
    } else if (fg->stk_top > v + 1) {
        Value v1 = v[1];
        RefNode *type = fs->Value_type(v1);

        if (type == fs->cls_int) {
            if (Value_isint(v1)) {
                rc->re = (double)Value_integral(v1);
            } else {
                RefInt *mp = Value_vp(v1);
                rc->re = fs->BigInt_double(&mp->bi);
                if (isinf(rc->re)) {
                    rc->re = 0.0;
                }
            }
        } else if (type == fs->cls_str || type == fs->cls_bytes) {
            // TODO
            /*
            RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
            RefStr *rs = Value_vp(v1);

            *vret = vp_Value(rd);
            errno = 0;
            rd->d = strtod(rs->c, NULL);
            if (errno != 0) {
                rd->d = 0.0;
            }
            */
        } else if (type == fs->cls_float) {
            rc->re = Value_float2(v1);
            return TRUE;
        } else if (type == fs->cls_frac) {
            RefFrac *md = Value_vp(v[1]);
            double d1 = fs->BigInt_double(&md->bi[0]);
            double d2 = fs->BigInt_double(&md->bi[1]);

            rc->re = d1 / d2;
            if (isinf(rc->re) || isnan(rc->re)) {
                rc->re = 0.0;
            }
            return TRUE;
        } else if (type == fs->cls_bool) {
            rc->re = (Value_bool(v1) ? 1.0 : 0.0);
        } else if (type == cls_complex) {
            const RefComplex *rc1 = Value_vp(v1);
            rc->re = rc1->re;
            rc->im = rc1->im;
        }
    } else {
        // 0+0i
    }

    return TRUE;
}
static int complex_polar(Value *vret, Value *v, RefNode *node)
{
    double r = Value_float2(v[1]);
    double theta = Value_float2(v[2]);
    RefComplex *rc = fs->buf_new(cls_complex, sizeof(RefComplex));
    *vret = vp_Value(rc);

    rc->re = r * cos(theta);
    rc->im = r * sin(theta);

    return TRUE;
}
static int append_float_tostr(StrBuf *sb, double val, Value *argv, int argc, int add_sign)
{
    int i;
    RefStr *rs;
    RefNode *ret_type;

    *fg->stk_top++ = fs->float_Value(fs->cls_float, val);
    for (i = 0; i < argc; i++) {
        *fg->stk_top++ = fs->Value_cp(argv[i]);
    }
    if (!fs->call_member_func(fs->str_tostr, argc, TRUE)) {
        return FALSE;
    }
    ret_type = fs->Value_type(fg->stk_top[-1]);
    if (ret_type != fs->cls_str) {
        fs->throw_error_select(THROW_RETURN_TYPE__NODE_NODE, fs->cls_str, ret_type);
        return FALSE;
    }
    rs = Value_vp(fg->stk_top[-1]);
    if (add_sign) {
        if (rs->size >= 1 && rs->c[0] != '+' && rs->c[0] != '-') {
            fs->StrBuf_add_c(sb, '+');
        }
    }
    fs->StrBuf_add(sb, rs->c, rs->size);
    fs->Value_pop();

    return TRUE;
}
static int complex_tostr(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc = Value_vp(*v);
    StrBuf sb;
    int ret;

    fs->StrBuf_init_refstr(&sb, 16);
    fs->StrBuf_add_c(&sb, '(');
    ret = append_float_tostr(&sb, rc->re, v + 1, fg->stk_top - v - 1, FALSE);
    if (ret) {
        ret = append_float_tostr(&sb, rc->im, v + 1, fg->stk_top - v - 1, TRUE);
    }
    if (ret) {
        fs->StrBuf_add(&sb, "i)", 2);
        *vret = fs->StrBuf_str_Value(&sb, fs->cls_str);
    } else {
        StrBuf_close(&sb);
        return FALSE;
    }
    return TRUE;
}
static int complex_eq(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc1 = Value_vp(*v);
    const RefComplex *rc2 = Value_vp(v[1]);

    *vret = bool_Value(rc1->re == rc2->re && rc1->im == rc2->im);

    return TRUE;
}
static int complex_hash(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc = Value_vp(*v);
    uint32_t *i32 = (uint32_t*)&rc->re;
    uint32_t hash = 0;
    int i;
    
    for (i = 0; i < 4; i++) {
        hash = hash * 31 + i32[i];
    }
    *vret = int32_Value(hash & INT32_MAX);

    return TRUE;
}
static int complex_empty(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc = Value_vp(*v);

    *vret = bool_Value(rc->re == 0.0 && rc->im == 0.0);

    return TRUE;
}
static int complex_marshal_read(Value *vret, Value *v, RefNode *node)
{
    uint8_t data[16];
    int rd_size = 16;

    RefComplex *rc = fs->buf_new(cls_complex, sizeof(RefComplex));
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    *vret = vp_Value(rc);

    if (!fs->stream_read_data(r, NULL, (char*)data, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }

    rc->re = bytes_to_double(data + 0);
    rc->im = bytes_to_double(data + 8);

    if (!complex_isvalid(rc)) {
        return FALSE;
    }
    return TRUE;
}
static int complex_marshal_write(Value *vret, Value *v, RefNode *node)
{
    RefComplex *rc = Value_vp(*v);
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint8_t data[16];

    double_to_bytes(data + 0, rc->re);
    double_to_bytes(data + 8, rc->im);
    if (!fs->stream_write_data(w, (const char*)data, 16)) {
        return FALSE;
    }

    return TRUE;
}

static int complex_part(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc = Value_vp(*v);

    if (FUNC_INT(node)) {
        *vret = fs->float_Value(fs->cls_float, rc->im);
    } else {
        *vret = fs->float_Value(fs->cls_float, rc->re);
    }
    return TRUE;
}
static int complex_rho(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc = Value_vp(*v);
    double r = sqrt(rc->re * rc->re + rc->im * rc->im);
    *vret = fs->float_Value(fs->cls_float, r);
    return TRUE;
}
static int complex_theta(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc = Value_vp(*v);
    double phi = atan2(rc->im, rc->re);
    *vret = fs->float_Value(fs->cls_float, phi);
    return TRUE;
}
static int complex_conj(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc = Value_vp(*v);
    RefComplex *rc2 = fs->buf_new(cls_complex, sizeof(RefComplex));
    *vret = vp_Value(rc2);

    rc2->re = rc->re;
    rc2->im = -rc->im;

    return TRUE;
}
static int complex_minus(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc1 = Value_vp(*v);
    RefComplex *rc = fs->buf_new(cls_complex, sizeof(RefComplex));
    *vret = vp_Value(rc);

    rc->re = -rc1->re;
    rc->im = -rc1->im;

    return TRUE;
}
static int complex_addsub(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc1 = Value_vp(*v);
    const RefComplex *rc2 = Value_vp(v[1]);
    RefComplex *rc = fs->buf_new(cls_complex, sizeof(RefComplex));
    *vret = vp_Value(rc);

    if (FUNC_INT(node)) {
        rc->re = rc1->re - rc2->re;
        rc->im = rc1->im - rc2->im;
    } else {
        rc->re = rc1->re + rc2->re;
        rc->im = rc1->im + rc2->im;
    }
    return complex_isvalid(rc);
}
static int complex_mul(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc1 = Value_vp(*v);
    const RefComplex *rc2 = Value_vp(v[1]);
    RefComplex *rc = fs->buf_new(cls_complex, sizeof(RefComplex));
    *vret = vp_Value(rc);

    if (rc1->im == 0.0) {
        rc->re = rc1->re * rc2->re;
        rc->im = rc1->re * rc2->im;
    } else if (rc2->im == 0.0) {
        rc->re = rc1->re * rc2->re;
        rc->im = rc1->im * rc2->re;
    } else {
        rc->re = rc1->re * rc2->re - rc1->im * rc2->im;
        rc->im = rc1->im * rc2->re + rc1->re * rc2->im;
    }

    return complex_isvalid(rc);
}
static int complex_div(Value *vret, Value *v, RefNode *node)
{
    const RefComplex *rc1 = Value_vp(*v);
    const RefComplex *rc2 = Value_vp(v[1]);
    RefComplex *rc = fs->buf_new(cls_complex, sizeof(RefComplex));
    *vret = vp_Value(rc);

    if (rc2->im != 0.0) {
        double norm2 = rc2->re * rc2->re + rc2->im * rc2->im;
        rc->re = (rc1->re * rc2->re + rc1->im * rc2->im) / norm2;
        rc->im = (rc1->im * rc2->re - rc1->re * rc2->im) / norm2;
    } else {
        rc->re = rc1->re / rc2->re;
        rc->im = rc1->im / rc2->re;
    }

    return complex_isvalid(rc);
}

/////////////////////////////////////////////////////////////////////////////////////

static int math_cfunc1(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefNode *v1_type = fs->Value_type(v1);
    const MathCFunc1 *f = FUNC_VP(node);

    if (v1_type == fs->cls_float) {
        double arg = Value_float2(v1);
        double ret;

        errno = 0;
        ret = f->real(arg);
        if (isnan(ret) || isinf(ret)) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal argument value");
            return FALSE;
        }
        *vret = fs->float_Value(fs->cls_float, ret);
    } else if (v1_type == cls_complex) {
        RefComplex *ret = fs->buf_new(cls_complex, sizeof(RefComplex));
        *vret = vp_Value(ret);

        errno = 0;
        if (!f->complex(ret, Value_vp(v1))) {
            if (fg->error == VALUE_NULL) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal argument value");
            }
            return FALSE;
        }
        if (!complex_isvalid(ret)) {
            return FALSE;
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_float, cls_complex, v1_type, 1);
        return FALSE;
    }
    if (errno != 0) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "%s", strerror(errno));
        return FALSE;
    }

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
    *vret = fs->float_Value(fs->cls_float, ret);

    return TRUE;
}

static int c_sqrt(RefComplex *r, const RefComplex *v1)
{
    double re = sqrt(v1->re * v1->re + v1->im * v1->im);
    double re_2 = sqrt(re);
    double arg = atan2(v1->im, v1->re) / 2.0;
    r->re = re_2 * cos(arg);
    r->im = re_2 * sin(arg);
/*
    if (v1->re > 0) {
        r->re = sqrt(0.5 * re_2 + 0.5 * v1->re);
        r->im = fabs(0.5 * v1->im / r->re);
    } else {
        r->im = sqrt(0.5 * re_2 - 0.5 * v1->re);
        r->re = fabs(0.5 * v1->im / r->im);
    }
    if (v1->im < 0) {
        r->im = -r->im;
    }
*/
    return TRUE;
}
static int c_exp(RefComplex *r, const RefComplex *v1)
{
    double re = exp(v1->re);
    r->re = re * cos(v1->im);
    r->im = re * sin(v1->im);
    return TRUE;
}
static int c_log(RefComplex *r, const RefComplex *v1)
{
    double re = sqrt(v1->re * v1->re + v1->im * v1->im);
    double arg = atan2(v1->im, v1->re);
    r->re = log(re);
    r->im = arg;
    return TRUE;
}
static int c_sin(RefComplex *r, const RefComplex *v1)
{
    r->re = sin(v1->re) * cosh(v1->im);
    r->im = cos(v1->re) * sinh(v1->im);
    return TRUE;
}
static int c_cos(RefComplex *r, const RefComplex *v1)
{
    r->re = cos(v1->re) * cosh(v1->im);
    r->im = -sin(v1->re) * sinh(v1->im);
    return TRUE;
}
static int c_tan(RefComplex *r, const RefComplex *v1)
{
    double sin_re = sin(v1->re) * cosh(v1->im);
    double sin_im = cos(v1->re) * sinh(v1->im);
    double cos_re = cos(v1->re) * cosh(v1->im);
    double cos_im = -sin(v1->re) * sinh(v1->im);

    double norm2 = cos_re * cos_re + cos_im * cos_im;
    r->re = (sin_re * cos_re + sin_im * cos_im) / norm2;
    r->im = (sin_im * cos_re - sin_re * cos_im) / norm2;

    return TRUE;
}
// sqrt(1 - x^2)
static void sqrt_1_x2(double *rre, double *rim, double re, double im)
{
    double re2 = 1.0 - (re * re - im * im);
    double im2 = -2.0 * im * re;

    double r = sqrt(re2 * re2 + im2 * im2);
    double arg = atan2(im2, re2) * 0.5;
    *rre = sqrt(r) * cos(arg);
    *rim = sqrt(r) * sin(arg);
}
static int c_asin(RefComplex *r, const RefComplex *v1)
{
    // asin(x) = -i * log(i * x + sqrt_1_x2)
    double re1, im1;
    double re, arg;
    sqrt_1_x2(&re1, &im1, v1->re, v1->im);
    re1 -= v1->im;
    im1 += v1->re;

    re = log(sqrt(re1 * re1 + im1 * im1));
    arg = atan2(im1, re1);

    r->re = arg;
    r->im = -re;

    return TRUE;
}
static int c_acos(RefComplex *r, const RefComplex *v1)
{
    // acos(x) = pi / 2 + i * log(i * x + sqrt_1_x2)
    double re1, im1;
    double re, arg;
    sqrt_1_x2(&re1, &im1, v1->re, v1->im);
    re1 -= v1->im;
    im1 += v1->re;

    re = log(sqrt(re1 * re1 + im1 * im1));
    arg = atan2(im1, re1);

    r->re = M_PI_2 - arg;
    r->im = re;

    return TRUE;
}
static int c_atan(RefComplex *r, const RefComplex *v1)
{
    // atan(x) = i / 2 * (log(1 - i * x) - log(1 + i * x))
    double re1 = 1.0 + v1->im, im1 = -v1->re;
    double re2 = 1.0 - v1->im, im2 = v1->re;
    double re3, im3;

    re3 = log(sqrt(re1 * re1 + im1 * im1));
    im3 = atan2(im1, re1);

    re3 -= log(sqrt(re2 * re2 + im2 * im2));
    im3 -= atan2(im2, re2);

    r->re = -im3 * 0.5;
    r->im = re3 * 0.5;

    return TRUE;
}
static int c_sinh(RefComplex *r, const RefComplex *v1)
{
    // sinh(x) = (exp(x) - exp(-x)) / 2
    double r1 = exp(v1->re);
    double r2 = 1.0 / r1;

    r->re = (r1 - r2) * cos(v1->im) * 0.5;
    r->im = (r1 + r2) * sin(v1->im) * 0.5;

    return TRUE;
}
static int c_cosh(RefComplex *r, const RefComplex *v1)
{
    // cosh(x) = (exp(x) + exp(-x)) / 2
    double r1 = exp(v1->re);
    double r2 = 1.0 / r1;

    r->re = (r1 + r2) * cos(v1->im) * 0.5;
    r->im = (r1 - r2) * sin(v1->im) * 0.5;

    return TRUE;
}
static int c_tanh(RefComplex *r, const RefComplex *v1)
{
    // tanh(x) = (exp(x) - exp(-x)) / (exp(x) + exp(-x))
    double r1 = exp(v1->re);
    double r2 = 1.0 / r1;
    double cos1 = cos(v1->im);
    double sin1 = sin(v1->im);

    double re1 = (r1 - r2) * cos1;
    double im1 = (r1 + r2) * sin1;
    double re2 = (r1 + r2) * cos1;
    double im2 = (r1 - r2) * sin1;

    if (im2 != 0.0) {
        double norm2 = re2 * re2 + im2 * im2;
        r->re = (re1 * re2 + im1 * im2) / norm2;
        r->im = (im1 * re2 - re1 * im2) / norm2;
    } else {
        r->re = re1 / re2;
        r->im = im1 / re2;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////

static void define_const(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "M_PI", NODE_CONST, 0);
    n->u.k.val = fs->float_Value(fs->cls_float, M_PI);

    n = fs->define_identifier(m, m, "M_E", NODE_CONST, 0);
    n->u.k.val = fs->float_Value(fs->cls_float, M_E);
}
static void define_math_cfunc1(RefNode *m)
{
    RefNode *n;
    int i;

    static const MathCFunc1 mf[] = {
        { "sqrt", sqrt, c_sqrt },
        { "exp", exp, c_exp },
        { "log", log, c_log },
        { "sin", sin, c_sin },
        { "cos", cos, c_cos },
        { "tan", tan, c_tan },
        { "asin", asin, c_asin },
        { "acos", acos, c_acos },
        { "atan", atan, c_atan },
        { "sinh", sinh, c_sinh },
        { "cosh", cosh, c_cosh },
        { "tanh", tanh, c_tanh },
    };
    
    for (i = 0; i < lengthof(mf); i++) {
        const MathCFunc1 *f = &mf[i];
        n = fs->define_identifier(m, m, f->name, NODE_FUNC_N, 0);
        fs->define_native_func_a(n, math_cfunc1, 1, 1, (MathCFunc1*)f, NULL);
    }
}
static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "atan2", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_func2, 2, 2, atan2, fs->cls_float, fs->cls_float);

    n = fs->define_identifier(m, m, "pow", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_pow, 2, 2, pow, NULL, NULL);

    n = fs->define_identifier(m, m, "abs", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_abs, 1, 1, NULL, NULL);

    n = fs->define_identifier(m, m, "gcd", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_gcd, 2, 2, NULL, fs->cls_int, fs->cls_int);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls = fs->define_identifier(m, m, "Complex", NODE_CLASS, 0);
    cls_complex = cls;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, complex_new, 0, 2, NULL, NULL, fs->cls_float);
    n = fs->define_identifier(m, cls, "polar", NODE_NEW_N, 0);
    fs->define_native_func_a(n, complex_polar, 0, 2, NULL, NULL, fs->cls_float);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, complex_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, complex_empty, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_eq, 1, 1, NULL, cls_complex);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, complex_hash, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier(m, cls, "real", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, complex_part, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "imag", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, complex_part, 0, 0, (void*)TRUE);
    n = fs->define_identifier(m, cls, "rho", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, complex_rho, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "theta", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, complex_theta, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_INV], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_conj, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_PLUS], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_native_return_this, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MINUS], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_minus, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_addsub, 1, 1, (void*)FALSE, cls_complex);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_addsub, 1, 1, (void*)TRUE, cls_complex);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_mul, 1, 1, NULL, cls_complex);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, complex_div, 1, 1, NULL, cls_complex);

    fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_math = m;

    define_class(m);
    define_vector_class(m);
    define_func(m);
    define_math_cfunc1(m);
    define_rand_func(m);
    define_vector_func(m);
    define_const(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}

