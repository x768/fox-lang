#define DEFINE_GLOBALS
#include "image.h"
#include "compat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


enum {
    FLAGS_ALPHA = 1,
    FLAGS_RESAMPLED = 2,
};

typedef struct {
    uint32_t color;
    const char *name;
} ColorName;

static RefNode *cls_image;
static RefNode *cls_fileio;

static ColorName color_names[] = {
    { 0xFF000000, "BLACK" },
    { 0xFF808080, "GRAY" },
    { 0xFFC0C0C0, "SILVER" },
    { 0xFFFFFFFF, "WHITE" },
    { 0xFFFF0000, "BLUE" },
    { 0xFF00FF00, "LIME" },
    { 0xFFFFFF00, "AQUA" },
    { 0xFF0000FF, "RED" },
    { 0xFFFF00FF, "FUCHSIA" },
    { 0xFF00FFFF, "YELLOW" },
    { 0xFF800000, "NAVY" },
    { 0xFF008000, "GREEN" },
    { 0xFF808000, "TEAL" },
    { 0xFF000080, "MAROON" },
    { 0xFF800080, "PURPLE" },
    { 0xFF008080, "OLIVE" },
};

static void throw_already_closed(void)
{
    fs->throw_errorf(mod_image, "ImageError", "Image is already closed");
}
static void throw_image_large(void)
{
    fs->throw_errorf(mod_image, "ImageError", "Image size too large (max:%d)", MAX_IMAGE_SIZE);
}
static int name_to_bands(int *bands, int *channels, RefStr *rs)
{
    if (str_eqi(rs->c, rs->size, "L", -1)) {
        *bands = BAND_L;
        *channels = 1;
    } else if (str_eqi(rs->c, rs->size, "P", -1)) {
        *bands = BAND_P;
        *channels = 1;
    } else if (str_eqi(rs->c, rs->size, "LA", -1)) {
        *bands = BAND_LA;
        *channels = 2;
    } else if (str_eqi(rs->c, rs->size, "RGB", -1)) {
        *bands = BAND_RGB;
        *channels = 3;
    } else if (str_eqi(rs->c, rs->size, "RGBA", -1)) {
        *bands = BAND_RGBA;
        *channels = 4;
    } else {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown image mode %q", rs->c);
        return FALSE;
    }
    return TRUE;
}

static const char *bands_to_name(int bands)
{
    switch (bands) {
    case BAND_L:
        return "L";
    case BAND_LA:
        return "LA";
    case BAND_P:
        return "P";
    case BAND_RGB:
        return "RGB";
    case BAND_RGBA:
        return "RGBA";
    }
    return "?";
}
static int char2hex(char ch)
{
    if (isdigit_fox(ch)) {
        return ch - '0';
    } else if (ch >= 'A' && ch <= 'F') {
        return 10 + ch - 'A';
    } else if (ch >= 'a' && ch <= 'f') {
        return 10 + ch - 'a';
    } else {
        return -1;
    }
}

// 256以上は255とする
// 数字以外が出現したらエラー
static int parse_digit255(int *result, Str src)
{
    const char *p = src.p;
    const char *end = p + src.size;
    int ret = 0;

    for (; p < end; p++) {
        if (!isdigit_fox(*p)) {
            return FALSE;
        }
        ret = ret * 10 + (*p - '0');
        if (ret > 255) {
            ret = 255;
        }
    }
    *result = ret;
    return TRUE;
}

// /\d(\.\d+)?/
static int parse_double(double *result, Str src)
{
    enum {
        S_MAX = 64,
    };
    const char *p = src.p;
    const char *end = p + src.size;
    char tmp[S_MAX];

    for (; p < end; p++) {
        if (!isdigit_fox(*p)) {
            break;
        }
    }
    if (p < end) {
        if (*p != '.') {
            return FALSE;
        }
        p++;
        for (; p < end; p++) {
            if (!isdigit_fox(*p)) {
                return FALSE;
            }
        }
    }
    if (src.size >= S_MAX) {
        memcpy(tmp, src.p, S_MAX - 1);
        tmp[S_MAX - 1] = '\0';
    } else {
        memcpy(tmp, src.p, src.size);
        tmp[src.size] = '\0';
    }

    *result = strtod(tmp, NULL);
    return TRUE;
}
static Str str_trim(const char *p, const char *end)
{
    Str s;

    while (p < end) {
        char c = *p;
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            break;
        }
        p++;
    }
    while (p < end) {
        char c = end[-1];
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            break;
        }
        end--;
    }
    s.p = p;
    s.size = end - p;
    return s;
}
static void color_to_hsl(double *ph, double *ps, double *pv, int color)
{
    double r = (double)((color & COLOR_R_MASK) >> COLOR_R_SHIFT) / 255.0;
    double g = (double)((color & COLOR_G_MASK) >> COLOR_G_SHIFT) / 255.0;
    double b = (double)((color & COLOR_B_MASK) >> COLOR_B_SHIFT) / 255.0;

    double max = (r > g ? (b > r ? b : r) : (b > g ? b : g));
    double min = (r < g ? (b < r ? b : r) : (b < g ? b : g));
    double v = (max + min) * 0.5;

    *pv = v;
    if (max == min) {
        *ph = 0.0;
        *ps = 0.0;
    } else {
        double cr = (max - r) / (max - min);
        double cg = (max - g) / (max - min);
        double cb = (max - b) / (max - min);
        double h;

        if (v >= 0.5) {
            *ps = (max - min) / (max + min) / (1.0 - v) * v;
        } else {
            *ps = (max - min) / (2.0 - max - min) / v * (1.0 - v);
        }
        if (r == max) {
            h = cb - cg;
        } else if (g == max) {
            h = 2.0 + cr - cb;
        } else {
            h = 4.0 + cg - cr;
        }
        h = 60.0 * h;
        if (h < 0.0) {
            h += 360.0;
        }
        *ph = h;
    }
}

static int color_gray(Value *vret, Value *v, RefNode *node)
{
    int64_t val64 = fs->Value_int64(v[1], NULL);
    int val;
    int alpha;

    if (val64 < 0) {
        val = 0;
    } else if (val64 > 255) {
        val = 255;
    } else {
        val = val64;
    }
    if (fg->stk_top > v + 2) {
        int64_t alpha64 = fs->Value_int64(v[2], NULL);
        if (alpha64 < 0) {
            alpha = 0;
        } else if (alpha64 > 255) {
            alpha = 255;
        } else {
            alpha = alpha64;
        }
    } else {
        alpha = 255;
    }
    *vret = integral_Value(cls_color, val | (val << 8) | (val << 16) | (alpha << 24));

    return TRUE;
}
static int color_rgb(Value *vret, Value *v, RefNode *node)
{
    uint32_t ret = 0;
    int argc = (fg->stk_top - fg->stk_base) - 1;
    int i;

    for (i = 0; i < argc; i++) {
        int val = fs->Value_int64(v[i + 1], NULL);

        if (val < 0) {
            val = 0;
        } else if (val > 255) {
            val = 255;
        }
        ret |= val << (i * 8);
    }
    if (argc == 3) {
        ret |= COLOR_A_MASK;
    }
    *vret = integral_Value(cls_color, ret);

    return TRUE;
}
static int color_hsl(Value *vret, Value *v, RefNode *node)
{
    double h = fs->Value_float(v[1]);
    double s = fs->Value_float(v[2]);
    double l = fs->Value_float(v[3]);
    uint32_t ret = color_hsl_sub(h, s, l);
    int a;

    if (fg->stk_top > v + 4) {
        int64_t val = fs->Value_int64(v[4], NULL);
        if (val < 0) {
            a = 0;
        } else if (val > 255) {
            a = 255;
        } else {
            a = (int)val;
        }
        ret |= a << COLOR_A_SHIFT;
    } else {
        ret |= COLOR_A_MASK;
    }
    *vret = integral_Value(cls_color, ret);

    return TRUE;
}
static int color_cmyk(Value *vret, Value *v, RefNode *node)
{
    int c = fs->Value_int64(v[1], NULL);
    int m = fs->Value_int64(v[2], NULL);
    int y = fs->Value_int64(v[3], NULL);
    int k = fs->Value_int64(v[4], NULL);

    int r = (255 - c) * (255 - k) / 255;
    int g = (255 - m) * (255 - k) / 255;
    int b = (255 - y) * (255 - k) / 255;
    
    if (r < 0) {
        r = 0;
    } else if (r > 255) {
        r = 255;
    }
    if (g < 0) {
        g = 0;
    } else if (g > 255) {
        g = 255;
    }
    if (b < 0) {
        b = 0;
    } else if (b > 255) {
        b = 255;
    }
    *vret = integral_Value(cls_color, (r << COLOR_R_SHIFT) | (g << COLOR_G_SHIFT) | (b << COLOR_B_SHIFT) | COLOR_A_MASK);

    return TRUE;
}
static int color_parse_name(uint32_t *ret, Str name)
{
    int i;
    char buf[16];

    if (name.size >= sizeof(buf)) {
        return FALSE;
    }

    for (i = 0; i < name.size; i++) {
        int ch = name.p[i];

        if (!isupper_fox(ch) && !islower_fox(ch)) {
            return FALSE;
        }
        if (islower_fox(ch)) {
            ch -= 'a' - 'A';
        }
        buf[i] = ch;
    }
    buf[i] = '\0';

    for (i = 0; i < (int)(sizeof(color_names) / sizeof(color_names[0])); i++) {
        if (strcmp(buf, color_names[i].name) == 0) {
            *ret = color_names[i].color;
            return TRUE;
        }
    }
    if (strcmp(buf, "transparent") == 0) {
        *ret = 0x000000;
        return TRUE;
    }
    return FALSE;
}
// 0 - 255
static int read_percent_or_255(int *result, const char **pp, const char *end)
{
    const char *top = *pp;
    const char *p = *pp;
    int i_ret;
    double d_ret;
    Str s;

    while (p < end && *p != ',') {
        p++;
    }
    s = str_trim(top, p);
    if (p < end) {
        *pp = p + 1;
    } else {
        *pp = p;
    }

    if (top < p && p[-1] == '%') {
        s.size--;
        if (parse_double(&d_ret, s)) {
            if (d_ret > 100.0) {
                i_ret = 255;
            } else {
                i_ret = (int)(d_ret * 255.0) / 100;
            }
            *result = i_ret;
            return TRUE;
        }
    } else {
        if (parse_digit255(&i_ret, s)) {
            *result = i_ret;
            return TRUE;
        }
    }
    return FALSE;
}
// 0.0 - 1.0
static int read_percent_or_float(double *result, const char **pp, const char *end, int degree)
{
    const char *top = *pp;
    const char *p = *pp;
    double d_ret;
    Str s;

    while (p < end && *p != ',') {
        p++;
    }
    s = str_trim(top, p);
    if (p < end) {
        *pp = p + 1;
    } else {
        *pp = p;
    }

    if (!degree && top < p && p[-1] == '%') {
        s.size--;
        if (parse_double(&d_ret, s)) {
            if (d_ret > 100.0) {
                d_ret = 1.0;
            } else {
                d_ret = d_ret / 100.0;
            }
            *result = d_ret;
            return TRUE;
        }
    } else {
        if (parse_double(&d_ret, s)) {
            if (!degree) {
                if (d_ret > 1.0) {
                    d_ret = 1.0;
                }
            }
            *result = d_ret;
            return TRUE;
        }
    }
    return FALSE;
}
// CSS3の色表記をparseする
static int color_parse(Value *vret, Value *v, RefNode *node)
{
    Str s = fs->Value_str(v[1]);
    int i;
    uint32_t ret = 0;

    if (color_parse_name(&ret, s)) {
        *vret = integral_Value(cls_color, ret);
        return TRUE;
    }

    if (s.size >= 4 && s.p[0] == '#') {
        if (s.size == 4) {
            for (i = 0; i < 3; i++) {
                int j = char2hex(s.p[i + 1]);
                if (j < 0) {
                    goto ERROR;
                }
                ret |= (j << (i * 8 + 4)) | (j << (i * 8));
            }
        } else if (s.size == 7) {
            for (i = 0; i < 3; i++) {
                int j = char2hex(s.p[i*2 + 1]);
                int k = char2hex(s.p[i*2 + 2]);
                if (j < 0 || k < 0) {
                    goto ERROR;
                }
                ret |= (j << (i * 8 + 4)) | (k << (i * 8));
            }
        } else {
            goto ERROR;
        }
        ret |= COLOR_A_MASK;
    } else if (s.size > 5 && (memcmp(s.p, "rgb(", 4) == 0 || memcmp(s.p, "rgba(", 5) == 0) && s.p[s.size - 1] == ')') {
        const char *p;
        const char *end = s.p + s.size - 1;
        if (s.p[3] == '(') {
            p = s.p + 4;
        } else {
            p = s.p + 5;
        }

        for (i = 0; i < 3; i++) {
            int j;
            if (!read_percent_or_255(&j, &p, end)) {
                goto ERROR;
            }
            ret |= j << (i * 8);
        }
        if (s.p[3] == '(') {
            ret |= COLOR_A_MASK;
        } else {
            double d;
            int j;
            if (!read_percent_or_float(&d, &p, end, FALSE)) {
                goto ERROR;
            }
            if (d > 1.0) {
                j = 255;
            } else {
                j = (int)(d * 255.0);
            }
            ret |= j << 24;
        }
        if (p < end) {
            goto ERROR;
        }
    } else if (s.size > 5 && (memcmp(s.p, "hsl(", 4) == 0 || memcmp(s.p, "hsla(", 5) == 0) && s.p[s.size - 1] == ')') {
        const char *p;
        const char *end = s.p + s.size - 1;
        double vh, vs, vl;
        if (s.p[3] == '(') {
            p = s.p + 4;
        } else {
            p = s.p + 5;
        }

        if (!read_percent_or_float(&vh, &p, end, TRUE)) {
            goto ERROR;
        }
        if (!read_percent_or_float(&vs, &p, end, FALSE)) {
            goto ERROR;
        }
        if (!read_percent_or_float(&vl, &p, end, FALSE)) {
            goto ERROR;
        }
        ret = color_hsl_sub(vh, vs, vl);

        if (s.p[3] == '(') {
            ret |= COLOR_A_MASK;
        } else {
            double d;
            int j;
            if (!read_percent_or_float(&d, &p, end, FALSE)) {
                goto ERROR;
            }
            if (d > 1.0) {
                j = 255;
            } else {
                j = (int)(d * 255.0);
            }
            ret |= j << 24;
        }
        if (p < end) {
            goto ERROR;
        }
    } else {
        goto ERROR;
    }

    *vret = integral_Value(cls_color, ret);

    return TRUE;
ERROR:
    fs->throw_errorf(fs->mod_lang, "ParseError", "Color.parse failed");
    return FALSE;
}
static int color_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint32_t ival;

    if (!fs->stream_read_uint32(r, &ival)) {
        return FALSE;
    }
    *vret = integral_Value(cls_color, ival);

    return TRUE;
}
static int color_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint32_t ival = Value_integral(*v);

    if (!fs->stream_write_uint32(w, ival)) {
        return FALSE;
    }
    return TRUE;
}
static int color_get_rgb(Value *vret, Value *v, RefNode *node)
{
    int ord = FUNC_INT(node);
    int val = fs->Value_int64(*v, NULL);

    *vret = int32_Value((val >> (ord * 8)) & 0xFF);

    return TRUE;
}
static int color_get_hsl(Value *vret, Value *v, RefNode *node)
{
    double ret[3];
    int ord = FUNC_INT(node);
    int val = fs->Value_int64(*v, NULL);
    RefFloat *rd = fs->buf_new(fs->cls_float, sizeof(RefFloat));
    *vret = vp_Value(rd);

    color_to_hsl(&ret[0], &ret[1], &ret[2], val);
    rd->d = ret[ord];

    return TRUE;
}

static int color_tostr(Value *vret, Value *v, RefNode *node)
{
    Str fmt;
    int val = Value_integral(*v);
    char buf[64];
    int r = (val & COLOR_R_MASK) >> COLOR_R_SHIFT;
    int g = (val & COLOR_G_MASK) >> COLOR_G_SHIFT;
    int b = (val & COLOR_B_MASK) >> COLOR_B_SHIFT;
    int a = (val & COLOR_A_MASK) >> COLOR_A_SHIFT;

    if (fg->stk_top > v + 1) {
        fmt = fs->Value_str(v[1]);
    } else {
        fmt.p = NULL;
        fmt.size = 0;
    }

    if (Str_eq_p(fmt, "")) {
        if (a == 255) {
            sprintf(buf, "Color(r=%d, g=%d, b=%d)", r, g, b);
        } else {
            sprintf(buf, "Color(r=%d, g=%d, b=%d, a=%d)", r, g, b, a);
        }
    } else if (Str_eq_p(fmt, "hex")) {
        sprintf(buf, "#%02x%02x%02x", r, g, b);
    } else if (Str_eq_p(fmt, "HEX")) {
        sprintf(buf, "#%02X%02X%02X", r, g, b);
    } else if (Str_eq_p(fmt, "x")) {
        sprintf(buf, "#%x%x%x", r >> 4, g >> 4, b >> 4);
    } else if (Str_eq_p(fmt, "X")) {
        sprintf(buf, "#%X%X%X", r >> 4, g >> 4, b >> 4);
    } else if (Str_eq_p(fmt, "rgb")) {
        sprintf(buf, "rgb(%d, %d, %d)", r, g, b);
    } else if (Str_eq_p(fmt, "rgb%")) {
        sprintf(buf, "rgb(%.1f%%, %.1f%%, %.1f%%)", (double)r / 2.55, (double)g / 2.55, (double)b / 2.55);
    } else if (Str_eq_p(fmt, "rgba")) {
        sprintf(buf, "rgba(%d, %d, %d, %.1f%%)", r, g, b, (double)a / 2.55);
    } else if (Str_eq_p(fmt, "rgba%")) {
        sprintf(buf, "rgba(%.1f%%, %.1f%%, %.1f%%, %.1f%%)", (double)r / 2.55, (double)g / 2.55, (double)b / 2.55, (double)a / 2.55);
    } else if (Str_eq_p(fmt, "hsl")) {
        double h, s, l;
        color_to_hsl(&h, &s, &l, val);
        sprintf(buf, "hsl(%.1f, %.1f%%, %.1f%%)", h, s * 100.0, l * 100.0);
    } else if (Str_eq_p(fmt, "hsla")) {
        double h, s, l;
        color_to_hsl(&h, &s, &l, val);
        sprintf(buf, "hsla(%.1f, %.1f%%, %.1f%%, %.1f%%)", h, s * 100.0, l * 100.0, (double)a / 2.55);
    } else {
        fs->throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", fmt);
        return FALSE;
    }
    *vret = fs->cstr_Value(fs->cls_str, buf, -1);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int rect_new(Value *vret, Value *v, RefNode *node)
{
    int i;
    RefNode *cls = FUNC_VP(node);
    RefRect *rc = fs->buf_new(cls, sizeof(RefRect));
    *vret = vp_Value(rc);

    for (i = 0; i < INDEX_RECT_NUM; i++) {
        Value vi = v[i + 1];
        if (Value_isref(vi)) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Parameter overflow (arguent #%d)", i + 1);
            return FALSE;
        }
        rc->i[i] = Value_integral(vi);
    }
    if (rc->i[INDEX_RECT_W] < 0 || rc->i[INDEX_RECT_H] < 0) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Negative value is not allowed");
        return FALSE;
    }

    return TRUE;
}
static int rect_marshal_read(Value *vret, Value *v, RefNode *node)
{
    int i;
    RefNode *cls = FUNC_VP(node);
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefRect *rc = fs->buf_new(cls, sizeof(RefRect));

    *vret = vp_Value(rc);
    for (i = 0; i < INDEX_RECT_NUM; i++) {
        uint32_t ival;
        if (!fs->stream_read_uint32(r, &ival)) {
            return FALSE;
        }
        rc->i[i] = (int32_t)ival;
    }

    return TRUE;
}
static int rect_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    const RefRect *rc = Value_vp(*v);
    int i;

    for (i = 0; i < INDEX_RECT_NUM; i++) {
        if (!fs->stream_write_uint32(w, (uint32_t)rc->i[i])) {
            return FALSE;
        }
    }
    return TRUE;
}
static int rect_get(Value *vret, Value *v, RefNode *node)
{
    const RefRect *rc = Value_vp(*v);
    int i = FUNC_INT(node);
    *vret = integral_Value(fs->cls_int, rc->i[i]);
    return TRUE;
}
static int rect_eq(Value *vret, Value *v, RefNode *node)
{
    const RefRect *rc1 = Value_vp(*v);
    const RefRect *rc2 = Value_vp(v[1]);
    int i;
    for (i = 0; i < 4; i++) {
        if (rc1->i[i] != rc2->i[i]) {
            *vret = VALUE_FALSE;
            return TRUE;
        }
    }
    *vret = VALUE_TRUE;
    return TRUE;
}
static int rect_hash(Value *vret, Value *v, RefNode *node)
{
    const RefRect *rc = Value_vp(*v);
    uint32_t hash = 0;
    int i;
    for (i = 0; i < 4; i++) {
        hash = hash * 31 + rc->i[i];
    }
    *vret = int32_Value(hash & INT32_MAX);
    return TRUE;
}
static int rect_tostr(Value *vret, Value *v, RefNode *node)
{
    const RefRect *rc = Value_vp(*v);
    char buf[128];
    sprintf(buf, "Rect(%d, %d, %d, %d)", rc->i[0], rc->i[1], rc->i[2], rc->i[3]);
    *vret = fs->cstr_Value(fs->cls_str, buf, -1);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int palette_new(Value *vret, Value *v, RefNode *node)
{
    int i;
    RefNode *cls_palette = FUNC_VP(node);
    RefArray *ra = fs->refarray_new(PALETTE_NUM);
    Value *v1 = v + 1;
    int argc = fg->stk_top - v1;
    Value trans;
    *vret = vp_Value(ra);

    if (argc == 1) {
        RefNode *v1_type = fs->Value_type(*v1);
        if (v1_type == fs->cls_list || v1_type == cls_palette) {
            RefArray *r1 = Value_vp(*v1);
            v1 = r1->p;
            argc = r1->size;
        }
    }
    if (argc > PALETTE_NUM) {
        fs->throw_errorf(fs->mod_lang, "ArgumentError", "256 or less arguments excepted (%d given)", argc);
        return FALSE;
    }
    for (i = 0; i < argc; i++) {
        RefNode *type = fs->Value_type(v1[i]);
        if (type != cls_color) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, cls_color, type, i);
            return FALSE;
        }
    }

    ra->rh.type = cls_palette;
    memcpy(ra->p, v1, argc * sizeof(Value));

    trans = integral_Value(cls_color, 0);
    for (i = argc; i < PALETTE_NUM; i++) {
        ra->p[i] = trans;
    }

    return TRUE;
}
static int palette_dispose(Value *vret, Value *v, RefNode *node)
{
    RefArray *r = Value_vp(*v);

    if (r->p != NULL) {
        free(r->p);
        r->p = NULL;
    }

    return TRUE;
}
static int palette_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefNode *cls_palette = FUNC_VP(node);
    RefArray *ra = fs->refarray_new(PALETTE_NUM);
    int i;
    *vret = vp_Value(ra);

    ra->rh.type = cls_palette;
    for (i = 0; i < PALETTE_NUM; i++) {
        uint32_t ival;
        if (!fs->stream_read_uint32(r, &ival)) {
            return FALSE;
        }
        ra->p[i] = integral_Value(cls_color, ival);
    }

    return TRUE;
}
static int palette_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefArray *ra = Value_vp(*v);
    int i;

    for (i = 0; i < PALETTE_NUM; i++) {
        uint32_t ival = Value_integral(ra->p[i]);
        if (!fs->stream_write_uint32(w, ival)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int palette_dup(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_palette = FUNC_VP(node);
    RefArray *r = Value_vp(*v);
    RefArray *ra = fs->refarray_new(PALETTE_NUM);
    *vret = vp_Value(ra);

    memcpy(ra->p, r->p, sizeof(Value) * PALETTE_NUM);
    ra->rh.type = cls_palette;

    return TRUE;
}
static int palette_eq(Value *vret, Value *v, RefNode *node)
{
    RefArray *r1 = Value_vp(*v);
    RefArray *r2 = Value_vp(v[1]);
    int i;

    for (i = 0; i < PALETTE_NUM; i++) {
        if (r1->p[i] != r2->p[i]) {
            *vret = VALUE_FALSE;
            return TRUE;
        }
    }
    *vret = VALUE_TRUE;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static Hash *load_type_function(Mem *mem)
{
    static Hash mods;

    if (mods.entry == NULL) {
        fs->Hash_init(&mods, mem, 64);
        fs->load_aliases_file(&mods, "data" SEP_S "image-type.txt");
    }
    return &mods;
}
static int image_load_from_reader(Value image, Value r, RefStr *type, int info_only)
{
    Hash *mods = load_type_function(&fg->st_mem);
    RefNode *fn;

    if (!fs->get_loader_function(&fn, type->c, type->size, "load_", mods)) {
        return FALSE;
    }
    fs->Value_push("Nvvb", image, r, info_only);
    if (!fs->call_function(fn, 3)) {
        return FALSE;
    }
    fs->Value_pop();
    return TRUE;
}

static int image_save_to_writer(Value image, Value w, RefStr *type, Value param)
{
    Hash *mods = load_type_function(&fg->st_mem);
    RefNode *fn;

    if (!fs->get_loader_function(&fn, type->c, type->size, "save_", mods)) {
        return FALSE;
    }
    fs->Value_push("Nvv", image, w);
    if (param != VALUE_NULL) {
        fs->Value_push("v", param);
    } else {
        *fg->stk_top++ = vp_Value(fs->refmap_new(0));
    }
    if (!fs->call_function(fn, 3)) {
        return FALSE;
    }
    fs->Value_pop();

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int image_new(Value *vret, Value *v, RefNode *node)
{
    int err = FALSE;
    int64_t width = fs->Value_int64(v[1], &err);
    int64_t height = fs->Value_int64(v[2], &err);
    size_t size_alloc;
    int channels;
    int bands;
    RefImage *image;
    RefArray *ref_pal = NULL;

    if (err) {
        throw_image_large();
        return FALSE;
    }
    if (width < 1 || height < 1) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "width and height must be more than 0");
        return FALSE;
    }
    if (width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        throw_image_large();
        return FALSE;
    }
    if (!name_to_bands(&bands, &channels, Value_vp(v[3]))) {
        return FALSE;
    }

    size_alloc = width * height * channels;
    if (size_alloc > fs->max_alloc) {
        fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    if (fg->stk_top > v + 4) {
        RefNode *type = fs->Value_type(v[4]);
        RefNode *cls_palette = FUNC_VP(node);

        if (type == cls_palette) {
            ref_pal = Value_vp(v[4]);
        } else if (type != fs->cls_null) {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, cls_palette, fs->cls_null, type, 4);
            return FALSE;
        }
    }

    image = fs->buf_new(cls_image, sizeof(RefImage));
    *vret = vp_Value(image);

    image->width = width;
    image->height = height;
    image->pitch = width * channels;

    image->data = malloc(size_alloc);
    memset(image->data, 0, size_alloc);
    image->bands = bands;

    if (bands == BAND_P) {
        int i;
        image->palette = malloc(sizeof(uint32_t) * PALETTE_NUM);
        if (ref_pal != NULL) {
            for (i = 0; i < PALETTE_NUM; i++) {
                image->palette[i] = Value_integral(ref_pal->p[i]);
            }
        } else {
            for (i = 0; i < PALETTE_NUM; i++) {
                image->palette[i] = COLOR_A_MASK;
            }
        }
    }
    return TRUE;
}

static int detect_image_type(RefStr **ret, Value v)
{
    char buf[MAGIC_MAX];
    int size = MAGIC_MAX;

    if (!fs->stream_read_data(v, NULL, buf, &size, TRUE, FALSE)) {
        return FALSE;
    }
    *ret = fs->mimetype_from_magic(buf, size);
    return TRUE;
}
static int image_load(Value *vret, Value *v, RefNode *node)
{
    Value reader;
    RefStr *type = NULL;

    if (!fs->value_to_streamio(&reader, v[1], FALSE, 0)) {
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        RefNode *v2_type = fs->Value_type(v[2]);

        if (v2_type == fs->cls_mimetype) {
            type = Value_vp(v[2]);
        } else if (v2_type == fs->cls_str) {
            type = fs->mimetype_from_name_refstr(Value_vp(v[2]));
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_mimetype, fs->cls_str, v2_type, 2);
            return FALSE;
        }
    }

    *vret = vp_Value(fs->buf_new(cls_image, sizeof(RefImage)));
    if (type == NULL && !detect_image_type(&type, reader)) {
        fs->Value_dec(reader);
        return FALSE;
    }
    if (!image_load_from_reader(*vret, reader, type, FUNC_INT(node))) {
        fs->Value_dec(reader);
        return FALSE;
    }
    fs->Value_dec(reader);

    return TRUE;
}

static int image_save(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    Value v1 = v[1];
    Value writer;
    Value param = VALUE_NULL;
    RefStr *type = NULL;

    if (image->data == NULL) {
        throw_already_closed();
        return FALSE;
    }

    if (fg->stk_top > v + 2) {
        RefNode *v2_type = fs->Value_type(v[2]);

        if (v2_type == fs->cls_mimetype) {
            type = Value_vp(v[2]);
        } else if (v2_type == fs->cls_str) {
            type = fs->mimetype_from_name_refstr(Value_vp(v[2]));
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_mimetype, fs->cls_str, v2_type, 2);
            return FALSE;
        }
    } else if (type == NULL) {
        // typeを設定
        Str fname = fs->Value_str(v[1]);
        if (fname.size > 0) {
            type = fs->mimetype_from_suffix(fname);
        }
    }
    if (fg->stk_top > v + 3) {
        param = v[3];
    }

    if (!fs->value_to_streamio(&writer, v1, TRUE, 0)) {
        return FALSE;
    }

    if (type == NULL) {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Image type required");
        goto ERROR_END;
    }
    if (!image_save_to_writer(*v, writer, type, param)) {
        goto ERROR_END;
    }
    fs->Value_dec(writer);
    return TRUE;

ERROR_END:
    fs->Value_dec(writer);
    return FALSE;
}
static int image_close(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    if (image->data != NULL) {
        free(image->data);
        image->data = NULL;
    }
    if (image->palette != NULL) {
        free(image->palette);
        image->palette = NULL;
    }
    return TRUE;
}
static int image_tostr(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    char buf[96];
    int closed = (image->data == NULL);

    sprintf(buf, "Image(mode=%s, width=%d, height=%d%s)", bands_to_name(image->bands), image->width, image->height, (closed ? ", closed" : ""));
    *vret = fs->cstr_Value(fs->cls_str, buf, -1);

    return TRUE;
}
// フォーマットも含めて完全に同じならtrue
static int image_eq(Value *vret, Value *v, RefNode *node)
{
    RefImage *im1 = Value_vp(*v);
    RefImage *im2 = Value_vp(v[1]);
    int y;
    int width;

    if (im1->data == NULL || im2->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    *vret = VALUE_FALSE;

    // フォーマットの比較
    if (im1->width != im2->width || im1->height != im2->height || im1->bands != im2->bands) {
        return TRUE;
    }
    if (im1->bands == BAND_P) {
        // パレットの比較
        int i;
        for (i = 0; i < PALETTE_NUM; i++) {
            if (im1->palette[i] != im2->palette[i]) {
                return TRUE;
            }
        }
    }
    // 画素の比較
    width = im1->width * bands_to_channels(im1->bands);
    for (y = 0; y < im1->height; y++) {
        if (memcmp(im1->data + im1->pitch * y, im2->data + im2->pitch * y, width) != 0) {
            return TRUE;
        }
    }

    *vret = VALUE_TRUE;
    return TRUE;
}
static int image_hash(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    uint32_t ret = 0;
    int x, y;
    int width;

    if (image->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    ret = 65537 + image->width;
    ret = ret * 31 + image->height;
    ret = ret * 31 + image->bands;

    if (image->bands == BAND_P) {
        int i;
        for (i = 0; i < PALETTE_NUM; i++) {
            ret = ret * 31 + image->palette[i];
        }
    }

    width = image->width * bands_to_channels(image->bands);
    for (y = 0; y < image->height; y += 16) {
        const uint8_t *p = image->data + image->pitch * y;
        for (x = 0; x < width; x += 16) {
            ret = ret * 31 + p[x];
        }
    }
    *vret = integral_Value(fs->cls_int, ret & INTPTR_MAX);

    return TRUE;
}
static int image_combine(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_matrix = FUNC_VP(node);
    int width = 0, height = 0;
    Value *va = v + 1;
    int va_size = (fg->stk_top - v) - 1;
    RefMatrix *mat = NULL;
    int i;

    if (fs->Value_type(*va) == cls_matrix) {
        mat = Value_vp(*va);
        va++;
        va_size--;
    }

    // チェック
    for (i = 0; i < va_size; i++) {
        RefNode *v_type = fs->Value_type(va[i]);
        RefImage *img;
        if (v_type != cls_image) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, cls_image, v_type, 1);
            return FALSE;
        }
        img = Value_vp(va[i]);
        if (img->data == NULL) {
            throw_already_closed();
            return FALSE;
        }
        if (width == 0) {
            width = img->width;
            height = img->height;
        } else {
            if (width != img->width || height != img->height) {
                fs->throw_errorf(fs->mod_lang, "ImageError", "Image size mismatch");
                return FALSE;
            }
        }
    }
    if (mat != NULL) {
        if (mat->cols != va_size + 1) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Matrix has %d columns (%d expected)", mat->cols, va_size + 1);
            return FALSE;
        }
        if (mat->rows > 4) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Matrix has %d rows (must be <=4)", mat->rows);
            return FALSE;
        }
    } else {
        if (va_size > 4) {
            fs->throw_errorf(fs->mod_lang, "ArgumentError", "4 or less arguments excepted (%d given)", (int)(fg->stk_top - v) - 1);
            return FALSE;
        }
    }

    return TRUE;
}
static int image_marshal_read(Value *vret, Value *v, RefNode *node)
{
    uint32_t ival;
    uint8_t bands;
    int rd_size = 1;
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefImage *image = fs->buf_new(cls_image, sizeof(RefImage));
    *vret = vp_Value(image);

    if (!fs->stream_read_data(r, NULL, (char*)&bands, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    image->bands = bands;

    if (!fs->stream_read_uint32(r, &ival)) {
        return FALSE;
    }
    if (ival > MAX_IMAGE_SIZE) {
        throw_image_large();
        return FALSE;
    }
    image->width = ival;
    image->pitch = ival * bands_to_channels(image->bands);

    if (!fs->stream_read_uint32(r, &ival)) {
        return FALSE;
    }
    if (ival > MAX_IMAGE_SIZE) {
        throw_image_large();
        return FALSE;
    }

    if (image->bands == BAND_P) {
        int i;
        image->palette = malloc(sizeof(uint32_t) * PALETTE_NUM);
        for (i = 0; i < PALETTE_NUM; i++) {
            if (!fs->stream_read_uint32(r, &image->palette[i])) {
                return FALSE;
            }
        }
    }

    image->height = ival;
    if (!fs->stream_read_uint32(r, &ival)) {
        return FALSE;
    }
    if (ival > 0) {
        if (ival != image->pitch * image->height) {
            fs->throw_errorf(mod_image, "ImageError", "Data size is not valid");
            return FALSE;
        }
        rd_size = ival;
        image->data = malloc(rd_size);

        if (!fs->stream_read_data(r, NULL, (char*)image->data, &rd_size, FALSE, TRUE)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int image_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefImage *image = Value_vp(*v);
    char bands = image->bands;

    if (!fs->stream_write_data(w, &bands, 1)) {
        return FALSE;
    }
    if (!fs->stream_write_uint32(w, image->width)) {
        return FALSE;
    }
    if (!fs->stream_write_uint32(w, image->height)) {
        return FALSE;
    }
    if (image->bands == BAND_P) {
        int i;
        for (i = 0; i < PALETTE_NUM; i++) {
            if (!fs->stream_write_uint32(w, image->palette[i])) {
                return FALSE;
            }
        }
    }

    if (image->data != NULL) {
        int width = image->width * bands_to_channels(image->bands);
        int y;

        if (!fs->stream_write_uint32(w, width * image->height)) {
            return FALSE;
        }
        for (y = 0; y < image->height; y++) {
            if (!fs->stream_write_data(w, (const char*)image->data + image->pitch * y, width)) {
                return FALSE;
            }
        }
    } else {
        if (!fs->stream_write_uint32(w, 0)) {
            return FALSE;
        }
    }

    return TRUE;
}
static int image_dup(Value *vret, Value *v, RefNode *node)
{
    RefImage *src = Value_vp(*v);
    RefImage *dst = fs->buf_new(cls_image, sizeof(RefImage));
    *vret = vp_Value(dst);

    if (src->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    memcpy(dst, src, sizeof(RefImage));
    dst->data = malloc(src->pitch * src->height);
    memcpy(dst->data, src->data, src->pitch * src->height);

    if (src->palette != NULL) {
        dst->palette = malloc(sizeof(uint32_t) * PALETTE_NUM);
        memcpy(dst->palette, src->palette, sizeof(uint32_t) * PALETTE_NUM);
    }

    return TRUE;
}

static int image_index(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    int err = FALSE;
    int64_t x = fs->Value_int64(v[1], &err);
    int64_t y = fs->Value_int64(v[2], &err);
    uint32_t color;
    uint8_t *line;

    if (image->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (err || x < 0 || x >= image->width || y < 0 || y >= image->height) {
        fs->throw_errorf(fs->mod_lang, "IndexError", "Invalid position");
        return FALSE;
    }

    line = image->data + image->pitch * y;
    switch (image->bands) {
    case BAND_L: {
        uint8_t l = line[x];
        color = (l << COLOR_R_SHIFT) | (l << COLOR_G_SHIFT) | (l << COLOR_B_SHIFT) | COLOR_A_MASK;
        break;
    }
    case BAND_LA: {
        uint8_t l = line[x * 2 + 0];
        uint8_t a = line[x * 2 + 1];
        color = (l << COLOR_R_SHIFT) | (l << COLOR_G_SHIFT) | (l << COLOR_B_SHIFT) | (a << COLOR_A_SHIFT);
        break;
    }
    case BAND_RGB: {
        uint8_t r = line[x * 3 + 0];
        uint8_t g = line[x * 3 + 1];
        uint8_t b = line[x * 3 + 2];
        color = (r << COLOR_R_SHIFT) | (g << COLOR_G_SHIFT) | (b << COLOR_B_SHIFT) | COLOR_A_MASK;
        break;
    }
    case BAND_RGBA: {
        uint8_t r = line[x * 4 + 0];
        uint8_t g = line[x * 4 + 1];
        uint8_t b = line[x * 4 + 2];
        uint8_t a = line[x * 4 + 3];
        color = (r << COLOR_R_SHIFT) | (g << COLOR_G_SHIFT) | (b << COLOR_B_SHIFT) | (a << COLOR_A_SHIFT);
        break;
    }
    case BAND_P:
        if (FUNC_INT(node)) {
            color = image->palette[line[x]];
        } else {
            *vret = integral_Value(fs->cls_int, line[x]);
            return TRUE;
        }
        break;
    default:
        fs->fatal_errorf("Unknown image->bands (%d)", image->bands);
        return FALSE;
    }
    *vret = integral_Value(cls_color, color);

    return TRUE;
}
static int image_index_set(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    int err = FALSE;
    int64_t x = fs->Value_int64(v[1], &err);
    int64_t y = fs->Value_int64(v[2], &err);
    uint8_t *line;

    if (image->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (err || x < 0 || x >= image->width || y < 0 || y >= image->height) {
        fs->throw_errorf(fs->mod_lang, "IndexError", "Invalid position");
        return FALSE;
    }

    line = image->data + image->pitch * y;
    if (!image_color_value_to_buf(line, image, v[3], x)) {
        return FALSE;
    }

    return TRUE;
}
static int image_size(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    int ret;

    if (FUNC_INT(node)) {
        ret = image->height;
    } else {
        ret = image->width;
    }
    *vret = int32_Value(ret);

    return TRUE;
}
static int image_rect(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    RefNode *cls = FUNC_VP(node);
    RefRect *rc = fs->buf_new(cls, sizeof(RefRect));
    *vret = vp_Value(rc);

    rc->i[INDEX_RECT_X] = 0;
    rc->i[INDEX_RECT_Y] = 0;
    rc->i[INDEX_RECT_W] = image->width;
    rc->i[INDEX_RECT_H] = image->height;

    return TRUE;
}
static int image_mode(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    *vret = fs->cstr_Value(fs->cls_str, bands_to_name(image->bands), -1);
    return TRUE;
}
static int image_channels(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    *vret = int32_Value(bands_to_channels(image->bands));
    return TRUE;
}
static int image_palette(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    int i;
    uint32_t *pal = image->palette;

    if (pal != NULL) {
        RefArray *ra = fs->refarray_new(PALETTE_NUM);
        *vret = vp_Value(ra);
        ra->rh.type = FUNC_VP(node); // cls_palette
        
        for (i = 0; i < PALETTE_NUM; i++) {
            ra->p[i] = integral_Value(cls_color, pal[i]);
        }
    }
    return TRUE;
}
static int image_set_palette(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    uint32_t *pal = image->palette;

    if (pal != NULL) {
        RefArray *ra = Value_vp(v[1]);
        int i;

        for (i = 0; i < PALETTE_NUM; i++) {
            pal[i] = Value_integral(ra->p[i]);
        }
    } else {
        fs->throw_errorf(mod_image, "ImageError", "Image is not palette mode");
        return FALSE;
    }

    return TRUE;
}
static void image_split_sub(RefImage *dst, const RefImage *src, int channels, int i)
{
    int x, y;
    int width = src->width;
    int height = src->height;
    int pitch_s = src->pitch;
    int pitch_d = dst->pitch;

    for (y = 0; y < height; y++) {
        const uint8_t *src_p = &src->data[y * pitch_s];
        uint8_t *dst_p = &dst->data[y * pitch_d];

        for (x = 0; x < width; x++) {
            dst_p[x] = src_p[x * channels + i];
        }
    }
}
static int image_split(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(*v);
    int channels = bands_to_channels(image->bands);

    if (image->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (channels >= 2) {
        int i;
        RefArray *ra = fs->refarray_new(channels);
        *vret = vp_Value(ra);
        for (i = 0; i < channels; i++) {
            RefImage *img = fs->buf_new(cls_image, sizeof(RefImage));
            ra->p[i] = vp_Value(img);
            img->bands = BAND_L;
            img->width = image->width;
            img->height = image->height;
            img->pitch = image->width;
            img->data = malloc(img->pitch * img->height);
            image_split_sub(img, image, channels, i);
        }
    } else {
        fs->throw_errorf(mod_image, "ImageError", "Cannot split image which has 1 channel");
        return FALSE;
    }

    return TRUE;
}
static int image_convert(Value *vret, Value *v, RefNode *node)
{
    RefImage *src = Value_vp(*v);
    RefImage *dst;
    int dst_bands = BAND_ERROR;
    RefMatrix *mat;
    int ch_src = bands_to_channels(src->bands);
    int ch_dst;

    if (!name_to_bands(&dst_bands, &ch_dst, Value_vp(v[1]))) {
        return FALSE;
    }
    if (src->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (src->bands != BAND_P) {
        if (dst_bands == BAND_P) {
            fs->throw_errorf(mod_image, "ImageError", "Cannot convert truecolor to palette");
            return FALSE;
        }
    }
    if (fg->stk_top > v + 2) {
        mat = Value_vp(v[2]);
    } else {
        mat = NULL;
    }

    dst = fs->buf_new(cls_image, sizeof(RefImage));
    *vret = vp_Value(dst);
    dst->bands = dst_bands;
    dst->width = src->width;
    dst->height = src->height;
    dst->pitch = src->width * ch_dst;
    dst->data = malloc(dst->pitch * dst->height);

    if (src->bands == BAND_P && dst->bands == BAND_P) {
        // パレットのみ変更する
        memcpy(dst->data, src->data, dst->pitch * dst->height);
        dst->palette = malloc(sizeof(uint32_t) * PALETTE_NUM);
        memcpy(dst->palette, src->palette, sizeof(uint32_t) * PALETTE_NUM);

        if (mat != NULL) {
            if (!image_convert_palette(dst->palette, mat)) {
                return FALSE;
            }
        }
    } else if (src->bands == BAND_P) {
        // パレット→true_color
        if (!image_convert_to_fullcolor(dst, ch_dst, src, mat)) {
            return FALSE;
        }
    } else {
        if (!image_convert_sub(dst, ch_dst, src, ch_src, mat)) {
            return FALSE;
        }
    }
    return TRUE;
}

static int image_convert_self(Value *vret, Value *v, RefNode *node)
{
    RefImage *img = Value_vp(*v);
    int ch = bands_to_channels(img->bands);
    RefMatrix *mat = Value_vp(v[1]);

    if (img->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (img->bands == BAND_P) {
        // パレットのみ変更する
        if (!image_convert_palette(img->palette, mat)) {
            return FALSE;
        }
    } else {
        if (!image_convert_sub(img, ch, img, ch, mat)) {
            return FALSE;
        }
    }

    return TRUE;
}
static int image_quantize(Value *vret, Value *v, RefNode *node)
{
    RefImage *src = Value_vp(*v);
    RefImage *dst;
    int mode = QUANT_MODE_RGB;
    int set_palette = FALSE;
    int dither = FALSE;

    if (src->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (src->bands != BAND_RGB && src->bands != BAND_RGBA) {
        fs->throw_errorf(mod_image, "ImageError", "Cannot quantize this mode (must be RGB/RGBA)");
        return FALSE;
    }

    dst = fs->buf_new(cls_image, sizeof(RefImage));
    *vret = vp_Value(dst);
    dst->width = src->width;
    dst->height = src->height;
    dst->pitch = dst->width;
    dst->bands = BAND_P;
    dst->palette = malloc(sizeof(uint32_t) * PALETTE_NUM);
    memset(dst->palette, 0, sizeof(uint32_t) * PALETTE_NUM);
    dst->data = malloc(dst->width * dst->height);

    if (fg->stk_top > v + 1) {
        RefNode *v1_type = fs->Value_type(v[1]);
        RefNode *cls_palette = FUNC_VP(node);

        if (v1_type == fs->cls_str) {
            RefStr *rs = Value_vp(v[1]);
            if (str_eqi(rs->c, rs->size, "rgb", -1)) {
                mode = QUANT_MODE_RGB;
            } else if (str_eqi(rs->c, rs->size, "rgba", -1)) {
                mode = QUANT_MODE_RGBA;
            } else if (str_eqi(rs->c, rs->size, "colorkey", -1)) {
                mode = QUANT_MODE_KEY;
            } else {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Unknwon mode %q", rs->c);
                return FALSE;
            }
        } else if (v1_type == cls_palette) {
            int i;
            RefArray *ra = Value_vp(v[1]);

            for (i = 0; i < PALETTE_NUM; i++) {
                dst->palette[i] = Value_integral(ra->p[i]);
            }
            set_palette = TRUE;
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, cls_palette, v1_type);
            return FALSE;
        }
    }
    if (fg->stk_top > v + 2) {
        dither = Value_bool(v[2]);
    }
    if (set_palette) {
        quantize_palette(dst, src, dither);
    } else {
        quantize(dst, src, mode, dither);
    }
    return TRUE;
}
static int image_copy(Value *vret, Value *v, RefNode *node)
{
    int alpha = FUNC_INT(node);
    RefImage *src = Value_vp(v[1]);
    RefImage *dst = Value_vp(*v);
    int err = FALSE;
    int64_t x = fs->Value_int64(v[2], &err);
    int64_t y = fs->Value_int64(v[3], &err);
    RefRect *src_rect;
    RefRect src_rect_v;

    if (err) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Parameter overflow");
        return FALSE;
    }

    if (fg->stk_top > v + 4) {
        src_rect = Value_vp(v[4]);
    } else {
        src_rect_v.i[INDEX_RECT_X] = 0;
        src_rect_v.i[INDEX_RECT_Y] = 0;
        src_rect_v.i[INDEX_RECT_W] = src->width;
        src_rect_v.i[INDEX_RECT_H] = src->height;
        src_rect = &src_rect_v;
    }

    if (src->data == NULL || dst->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    copy_image_sub(dst, src, x, y, src_rect->i, alpha);

    return TRUE;
}
static int image_copy_resized(Value *vret, Value *v, RefNode *node)
{
    int flags = FUNC_INT(node);
    RefImage *src = Value_vp(v[1]);
    RefImage *dst = Value_vp(*v);
    RefRect *dst_rect;
    RefRect *src_rect;
    RefRect dst_rect_v;
    RefRect src_rect_v;

    if (fg->stk_top > v + 2) {
        dst_rect = Value_vp(v[2]);
    } else {
        dst_rect_v.i[INDEX_RECT_X] = 0;
        dst_rect_v.i[INDEX_RECT_Y] = 0;
        dst_rect_v.i[INDEX_RECT_W] = dst->width;
        dst_rect_v.i[INDEX_RECT_H] = dst->height;
        dst_rect = &dst_rect_v;
    }
    if (fg->stk_top > v + 3) {
        src_rect = Value_vp(v[3]);
    } else {
        src_rect_v.i[INDEX_RECT_X] = 0;
        src_rect_v.i[INDEX_RECT_Y] = 0;
        src_rect_v.i[INDEX_RECT_W] = src->width;
        src_rect_v.i[INDEX_RECT_H] = src->height;
        src_rect = &src_rect_v;
    }

    if (src->data == NULL || dst->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    copy_image_resized_sub(dst, src, dst_rect->i, src_rect->i, (flags & FLAGS_ALPHA), (flags & FLAGS_RESAMPLED));

    return TRUE;
}
static int image_fill_rect(Value *vret, Value *v, RefNode *node)
{
    RefImage *img = Value_vp(*v);
    int x, y;
    int x1, y1, x2, y2;
    uint8_t cbuf[4];
    int sc = bands_to_channels(img->bands);

    if (img->data == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (!image_color_value_to_buf(cbuf, img, v[1], 0)) {
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        RefRect *rc = Value_vp(v[2]);
        x1 = rc->i[INDEX_RECT_X];
        y1 = rc->i[INDEX_RECT_Y];
        x2 = rc->i[INDEX_RECT_W];
        y2 = rc->i[INDEX_RECT_H];
        if (x1 < 0) {
            x2 += x1;
            x1 = 0;
        } else if (x2 > img->width - x1) {
            x2 = img->width - x1;
        }
        if (y1 < 0) {
            y2 += y1;
            y1 = 0;
        } else if (y2 > img->height - y1) {
            y2 = img->height - y1;
        }
    } else {
        x1 = 0;
        y1 = 0;
        x2 = img->width;
        y2 = img->height;
    }

    for (y = 0; y < y2; y++) {
        uint8_t *p = img->data + (y + y1) * img->pitch + x1 * sc;
        for (x = 0; x < x2; x++) {
            int i;
            for (i = 0; i < sc; i++) {
                *p++ = cbuf[i];
            }
        }
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static void define_image_color_const(RefNode *m, RefNode *cls)
{
    RefNode *n;
    int i;

    for (i = 0; i < (int)(sizeof(color_names) / sizeof(color_names[0])); i++) {
        ColorName *c = &color_names[i];
        n = fs->define_identifier(m, cls, c->name, NODE_CONST, 0);
        n->u.k.val = integral_Value(cls, c->color);
    }

    n = fs->define_identifier(m, cls, "TRANSPARENT", NODE_CONST, 0);
    n->u.k.val = integral_Value(cls, 0x00000000);
}
static void define_palette_grayscale(RefNode *m, RefNode *cls)
{
    int i;
    RefNode *n = fs->define_identifier(m, cls, "GRAY", NODE_CONST, 0);
    RefArray *ra = fs->refarray_new(PALETTE_NUM);
    n->u.k.val = vp_Value(ra);
    ra->rh.type = cls;  // Palette

    for (i = 0; i < 256; i++) {
        uint32_t c = (i << COLOR_R_SHIFT) | (i << COLOR_G_SHIFT) | (i << COLOR_B_SHIFT) | COLOR_A_MASK;
        ra->p[i] = integral_Value(cls_color, c);
    }
}
static void define_palette_websafe(RefNode *m, RefNode *cls)
{
    int i = 0;
    int r, g, b;
    RefNode *n = fs->define_identifier(m, cls, "WEBSAFE", NODE_CONST, 0);
    RefArray *ra = fs->refarray_new(PALETTE_NUM);
    n->u.k.val = vp_Value(ra);
    ra->rh.type = cls;  // Palette

    for (b = 0; b < 0x100; b += 0x33) {
        for (g = 0; g < 0x100; g += 0x33) {
            for (r = 0; r < 0x100; r += 0x33) {
                uint32_t c = (r << COLOR_R_SHIFT) | (g << COLOR_G_SHIFT) | (b << COLOR_B_SHIFT) | COLOR_A_MASK;
                ra->p[i] = integral_Value(cls_color, c);
                i++;
            }
        }
    }
    for (; i < PALETTE_NUM; i++) {
        ra->p[i] = integral_Value(cls_color, COLOR_A_MASK);
    }
}
static void define_palette_heatmap(RefNode *m, RefNode *cls)
{
    int i;
    RefNode *n = fs->define_identifier(m, cls, "HEATMAP", NODE_CONST, 0);
    RefArray *ra = fs->refarray_new(PALETTE_NUM);
    n->u.k.val = vp_Value(ra);
    ra->rh.type = cls;  // Palette

    // 青→水色
    for (i = 0; i < 64; i++) {
        uint32_t c = ((i * 4) << COLOR_G_SHIFT) | COLOR_B_MASK | COLOR_A_MASK;
        ra->p[i] = integral_Value(cls_color, c);
    }
    // 水色→緑
    for (i = 0; i < 64; i++) {
        uint32_t c = (255 << COLOR_G_SHIFT) | ((255 - i * 4) << COLOR_B_SHIFT) | COLOR_A_MASK;
        ra->p[64 + i] = integral_Value(cls_color, c);
    }
    // 緑→黄色
    for (i = 0; i < 64; i++) {
        uint32_t c = ((i * 4) << COLOR_R_SHIFT) | (255 << COLOR_G_SHIFT) | COLOR_A_MASK;
        ra->p[128 + i] = integral_Value(cls_color, c);
    }
    // 黄色→赤
    for (i = 0; i < 64; i++) {
        uint32_t c = COLOR_R_MASK | ((255 - i * 4) << COLOR_G_SHIFT) | COLOR_A_MASK;
        ra->p[128 + 64 + i] = integral_Value(cls_color, c);
    }
}
static NativeFunc get_function_ptr(RefNode *node, RefStr *name)
{
    RefNode *n = fs->get_node_member(fs->cls_list, name, NULL);
    if (n == NULL || n->type != NODE_FUNC_N) {
        fs->fatal_errorf(NULL, "Initialize module 'image' failed at get_function_ptr(%r)", name);
    }
    return n->u.f.u.fn;
}

static void define_class(RefNode *m, RefNode *mod_math)
{
    RefNode *cls;
    RefNode *n;

    NativeFunc fn_array_indexof = get_function_ptr(fs->cls_list, fs->symbol_stock[T_IN]);

    RefNode *cls_matrix = fs->Hash_get(&mod_math->u.m.h, "Matrix", -1);
    RefNode *cls_rect = fs->define_identifier(m, m, "Rect", NODE_CLASS, 0);
    RefNode *cls_palette = fs->define_identifier(m, m, "Palette", NODE_CLASS, 0);
    cls_image = fs->define_identifier(m, m, "Image", NODE_CLASS, 0);
    cls_color = fs->define_identifier(m, m, "Color", NODE_CLASS, NODEOPT_INTEGRAL);

    cls = cls_color;
    n = fs->define_identifier(m, cls, "gray", NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_gray, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "rgb", NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_rgb, 3, 3, NULL, fs->cls_int, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "rgba", NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_rgb, 4, 4, NULL, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "hsl", NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_hsl, 3, 3, NULL, fs->cls_float, fs->cls_float, fs->cls_float);
    n = fs->define_identifier(m, cls, "hsla", NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_hsl, 4, 4, NULL, fs->cls_float, fs->cls_float, fs->cls_float, fs->cls_int);
    n = fs->define_identifier(m, cls, "cmyk", NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_cmyk, 4, 4, NULL, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_parse, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, color_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, color_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, color_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier(m, cls, "r", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, color_get_rgb, 0, 0, (void*) 0);
    n = fs->define_identifier(m, cls, "g", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, color_get_rgb, 0, 0, (void*) 1);
    n = fs->define_identifier(m, cls, "b", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, color_get_rgb, 0, 0, (void*) 2);
    n = fs->define_identifier(m, cls, "h", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, color_get_hsl, 0, 0, (void*) 0);
    n = fs->define_identifier(m, cls, "s", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, color_get_hsl, 0, 0, (void*) 1);
    n = fs->define_identifier(m, cls, "l", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, color_get_hsl, 0, 0, (void*) 2);
    n = fs->define_identifier(m, cls, "a", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, color_get_rgb, 0, 0, (void*) 3);

    define_image_color_const(m, cls);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_rect;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, rect_new, 4, 4, cls, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, rect_marshal_read, 1, 1, cls, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, rect_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, rect_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, rect_hash, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, rect_eq, 1, 1, NULL, cls_image);
    n = fs->define_identifier(m, cls, "x", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, rect_get, 0, 0, (void*) INDEX_RECT_X);
    n = fs->define_identifier(m, cls, "y", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, rect_get, 0, 0, (void*) INDEX_RECT_Y);
    n = fs->define_identifier(m, cls, "width", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, rect_get, 0, 0, (void*) INDEX_RECT_W);
    n = fs->define_identifier(m, cls, "height", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, rect_get, 0, 0, (void*) INDEX_RECT_H);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_palette;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, palette_new, 0, -1, cls);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, palette_marshal_read, 1, 1, cls, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, palette_dispose, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, palette_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, palette_eq, 1, 1, NULL, cls_palette);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->symbol_stock[T_LB]), 1, 1, NULL, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->symbol_stock[T_LET_B]), 2, 2, NULL, fs->cls_int, cls_color);
    n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_iterator), 0, 0, NULL);
    n = fs->define_identifier(m, cls, "entries", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->intern("entries", -1)), 0, 0, NULL);
    n = fs->define_identifier(m, cls, "keys", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->intern("keys", -1)), 0, 0, NULL);
    n = fs->define_identifier(m, cls, "values", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_iterator), 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, fn_array_indexof, 1, 1, (void*)FALSE, cls_color);
    n = fs->define_identifier(m, cls, "index_of", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, fn_array_indexof, 1, 1, (void*)TRUE, cls_color);
    n = fs->define_identifier(m, cls, "has_key", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->intern("has_key", -1)), 1, 1, NULL, fs->cls_int);
    n = fs->define_identifier(m, cls, "has_value", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, fn_array_indexof, 1, 1, (void*)FALSE, cls_color);
    n = fs->define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, palette_dup, 0, 0, cls_palette);
    n = fs->define_identifier(m, cls, "to_array", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, palette_dup, 0, 0, fs->cls_list);

    define_palette_grayscale(m, cls);
    define_palette_websafe(m, cls);
    define_palette_heatmap(m, cls);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_image;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, image_new, 3, 4, cls_palette, fs->cls_int, fs->cls_int, fs->cls_str, NULL);
    n = fs->define_identifier(m, cls, "load", NODE_NEW_N, 0);
    fs->define_native_func_a(n, image_load, 1, 2, (void*)FALSE, NULL, NULL);
    n = fs->define_identifier(m, cls, "load_info", NODE_NEW_N, 0);
    fs->define_native_func_a(n, image_load, 1, 2, (void*)TRUE, NULL, NULL);
    n = fs->define_identifier(m, cls, "combine", NODE_NEW_N, 0);
    fs->define_native_func_a(n, image_combine, 0, -1, cls_matrix);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, image_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, image_hash, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_eq, 1, 1, NULL, cls_image);
    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = fs->define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_dup, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "save", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_save, 1, 3, NULL, NULL, NULL, fs->cls_map);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_index, 2, 2, (void*)FALSE, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "at", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_index, 2, 2, (void*)FALSE, fs->cls_int, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_index_set, 3, 3, NULL, fs->cls_int, fs->cls_int, NULL);
    n = fs->define_identifier(m, cls, "width", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, image_size, 0, 0, (void*) FALSE);
    n = fs->define_identifier(m, cls, "height", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, image_size, 0, 0, (void*) TRUE);
    n = fs->define_identifier(m, cls, "rect", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, image_rect, 0, 0, cls_rect);
    n = fs->define_identifier(m, cls, "mode", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, image_mode, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "channels", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, image_channels, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "palette", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, image_palette, 0, 0, cls_palette);
    n = fs->define_identifier(m, cls, "palette=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_set_palette, 1, 1, NULL, cls_palette);
    n = fs->define_identifier(m, cls, "split", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_split, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "convert", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_convert, 1, 2, NULL, fs->cls_str, cls_matrix);
    n = fs->define_identifier(m, cls, "convert_self", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_convert_self, 1, 1, NULL, cls_matrix);
    n = fs->define_identifier(m, cls, "quantize", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_quantize, 0, 2, cls_palette, NULL, NULL);
    n = fs->define_identifier(m, cls, "copy", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_copy, 3, 4, (void*)FALSE, cls_image, fs->cls_int, fs->cls_int, cls_rect);
    n = fs->define_identifier(m, cls, "copy_resized", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_copy_resized, 1, 3, (void*)0, cls_image, cls_rect, cls_rect);
    n = fs->define_identifier(m, cls, "copy_resampled", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_copy_resized, 1, 3, (void*)FLAGS_RESAMPLED, cls_image, cls_rect, cls_rect);
    n = fs->define_identifier(m, cls, "blend", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_copy, 3, 4, (void*)TRUE, cls_image, fs->cls_int, fs->cls_int, cls_rect);
    n = fs->define_identifier(m, cls, "blend_resized", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_copy_resized, 1, 3, (void*)FLAGS_ALPHA, cls_image, cls_rect, cls_rect);
    n = fs->define_identifier(m, cls, "blend_resampled", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_copy_resized, 1, 3, (void*)(FLAGS_ALPHA|FLAGS_RESAMPLED), cls_image, cls_rect, cls_rect);
    n = fs->define_identifier(m, cls, "fill_rect", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_fill_rect, 1, 2, NULL, NULL, cls_rect);
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "ImageError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

static ImageStatic *ImageStatic_new(void)
{
    ImageStatic *f = fs->Mem_get(&fg->st_mem, sizeof(ImageStatic));

    f->CopyParam_init = CopyParam_init;
    f->copy_image_line = copy_image_line;

    return f;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    RefNode *mod_math;

    fs = a_fs;
    fg = a_fg;
    mod_image = m;
    mod_math = fs->get_module_by_name("math", -1, TRUE, FALSE);
    cls_fileio = fs->Hash_get(&fs->mod_io->u.m.h, "FileIO", -1);

    m->u.m.ext = ImageStatic_new();

    define_class(m, mod_math);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    fs = a_fs;
    if (buf == NULL) {
        Mem mem;
        Hash *hash;
        StrBuf sb;
        int i;
        int first = TRUE;

        fs->Mem_init(&mem, 256);
        hash = load_type_function(&mem);
        fs->StrBuf_init(&sb, 0);
        fs->StrBuf_add(&sb, "Build at\t" __DATE__ "\nSupported type\t", -1);

        for (i = 0; i < hash->entry_num; i++) {
            const HashEntry *he = hash->entry[i];
            for (; he != NULL; he = he->next) {
                if (first) {
                    first = FALSE;
                } else {
                    fs->StrBuf_add(&sb, ", ", 2);
                }
                fs->StrBuf_add_r(&sb, he->key);
            }
        }

        fs->StrBuf_add(&sb, "\n\0", 2);
        buf = sb.p;
        fs->Mem_close(&mem);
    }
    return buf;
}
