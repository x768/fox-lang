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


int parse_round_mode(RefStr *rs)
{
    if (rs->size == 1) {
        switch (tolower_fox(rs->c[0])) {
        case 'd':
            return ROUND_DOWN;
        case 'u':
            return ROUND_UP;
        case 'f':
            return ROUND_FLOOR;
        case 'c':
            return ROUND_CEILING;
        }
    } else if (rs->size == 2 && (rs->c[0] == 'h' || rs->c[0] == 'H')) {
        switch (tolower_fox(rs->c[1])) {
        case 'd':   // hd
            return ROUND_HALF_DOWN;
        case 'u':   // hu
            return ROUND_HALF_UP;
        case 'e':   // he
            return ROUND_HALF_EVEN;
        }
    } else if (str_eqi(rs->c, rs->size, "down", 4)) {
        return ROUND_DOWN;
    } else if (str_eqi(rs->c, rs->size, "up", 2)) {
        return ROUND_UP;
    } else if (str_eqi(rs->c, rs->size, "floor", 5)) {
        return ROUND_FLOOR;
    } else if (str_eqi(rs->c, rs->size, "ceiling", 7)) {
        return ROUND_CEILING;
    } else if (str_eqi(rs->c, rs->size, "halfdown", 8)) {
        return ROUND_HALF_DOWN;
    } else if (str_eqi(rs->c, rs->size, "halfup", 6)) {
        return ROUND_HALF_UP;
    } else if (str_eqi(rs->c, rs->size, "halfeven", 8)) {
        return ROUND_HALF_EVEN;
    }
    return ROUND_ERR;
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
int integer_tostr(Value *vret, Value *v, RefNode *node)
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

int frac_tostr(Value *vret, Value *v, RefNode *node)
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
int float_tostr(Value *vret, Value *v, RefNode *node)
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

/////////////////////////////////////////////////////////////////////////////////////////////////////

int char_tostr(Value *vret, Value *v, RefNode *node)
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
int base58encode(Value *vret, Value *v, RefNode *node)
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
int base58decode(Value *vret, Value *v, RefNode *node)
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
