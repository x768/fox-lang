#define DEFINE_GLOBALS
#include "m_unicode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_unicode;

enum {
    CODEPOINTS_PLANE = 0x10000,
    MAX_PLANE = 16,
    COMPO_HASH_SIZE = 128,
};
enum {
    BREAKITER_SRC,
    BREAKITER_TYPE,
    BREAKITER_LOCALE,
    BREAKITER_BEGIN,   // バイト数
    BREAKITER_END,     // バイト数
    BREAKITER_NUM,
};
enum {
    BREAKS_NONE,
    BREAKS_POINT,     // コードポイント
    BREAKS_CHAR,      // 書記素クラスタ
    BREAKS_WORD,
    BREAKS_SENTENCE,
    BREAKS_LINE,
};
enum {
    GRAPHEME_Other,
    GRAPHEME_CR,
    GRAPHEME_LF,
    GRAPHEME_Control,
    GRAPHEME_Extend,
    GRAPHEME_SpacingMark,
    GRAPHEME_L,
    GRAPHEME_V,
    GRAPHEME_T,
    GRAPHEME_LV,
    GRAPHEME_LVT,
    GRAPHEME_Regional_Indicator,
};

enum {
    HANGUL_S_BASE = 0xAC00,
    HANGUL_L_BASE = 0x1100,
    HANGUL_V_BASE = 0x1161,
    HANGUL_T_BASE = 0x11A7,
    HANGUL_L_COUNT = 19,
    HANGUL_V_COUNT = 21,
    HANGUL_T_COUNT = 28,
    HANGUL_N_COUNT = HANGUL_V_COUNT * HANGUL_T_COUNT,
    HANGUL_S_COUNT = HANGUL_L_COUNT * HANGUL_N_COUNT,
};

#define PTR_NO_DATA ((void*)1)
#define COMPO_HASH(c1, c2) (((c1) * 30013 + (c2) * 24007) & (COMPO_HASH_SIZE - 1))


typedef struct CompoHash {
    struct CompoHash *next;
    uint32_t c1, c2;  // 結合前
    uint32_t ch;      // 結合後
} CompoHash;


static uint8_t *ch_category[MAX_PLANE + 1];
static uint8_t *ch_grapheme[MAX_PLANE + 1];
static uint8_t *ch_combi_class[MAX_PLANE + 1];
static uint8_t *ch_normalize_c[MAX_PLANE + 1];
static uint8_t *ch_normalize_k[MAX_PLANE + 1];
static uint32_t *code_normalize;
static int compo_hash_loaded;
static CompoHash *compo_hash[COMPO_HASH_SIZE];

/**
 * http://unicode.org/Public/UNIDATA/auxiliary/GraphemeBreakTest.html
 * Unicode 9.0.0
 */
static const uint32_t grapheme_break[] = {
	0x00008050,
	0x00000004,
	0x00000000,
	0x00000000,
	0x00008050,
	0x0003fff1,
	0x00008050,
	0x00008dd0,
	0x00008350,
	0x00008250,
	0x00008350,
	0x00008250,
	0x00009050,
	0x0000c050,
	0x00008050,
	0x00038050,
	0x00008050,
	0x0000c050,
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * UTF8のコードポイント数
 * 不正・非最短形式の場合は未定義
 */
static int strlen_utf8(const char *ptr, int len)
{
    const unsigned char *p = (const unsigned char*)ptr;
    const unsigned char *end = p + len;
    int count = 0;

    for (; p < end; p++) {
        if ((*p & 0xC0) != 0x80){
            count++;
        }
    }
    return count;
}

/**
 * 配列末尾に文字列を追加
 */
static void refarray_push_str(RefArray *arr, const char *p, int size, RefNode *type)
{
    Value *v_tmp = fs->refarray_push(arr);
    *v_tmp = fs->cstr_Value(type, p, size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int name_to_break_type(RefStr *rs)
{
    switch (tolower_fox(rs->c[0])) {
    case 'p':
        if (rs->size == 1 || str_eqi(rs->c, rs->size, "point", -1)) {
            return BREAKS_POINT;
        }
        break;
    case 'c':
        if (rs->size == 1 || str_eqi(rs->c, rs->size, "char", -1)) {
            return BREAKS_CHAR;
        }
        break;
    case 'w':
        if (rs->size == 1 || str_eqi(rs->c, rs->size, "word", -1)) {
            return BREAKS_WORD;
        }
        break;
    case 's':
        if (rs->size == 1 || str_eqi(rs->c, rs->size, "sentence", -1)) {
            return BREAKS_SENTENCE;
        }
        break;
    case 'l':
        if (rs->size == 1 || str_eqi(rs->c, rs->size, "line", -1)) {
            return BREAKS_LINE;
        }
        break;
    default:
        break;
    }

    fs->throw_errorf(fs->mod_lang, "ValueError", "break-type must 'c'(char), 'w'(word), 's'(sentence) or 'l'(line)");
    return BREAKS_NONE;
}

static uint8_t *load_unicode_property(const char *type, int plane)
{
    int size;
    uint8_t *ret;
    char *path = fs->str_printf("%r" SEP_S "unicode" SEP_S "%s%i", fs->fox_home, type, plane);

    ret = (uint8_t*)fs->read_from_file(&size, path, &fg->st_mem);
    free(path);

    if (ret != NULL) {
        return ret;
    } else {
        return PTR_NO_DATA;
    }
}
static uint8_t *load_unicode_data(int *size, const char *name)
{
    uint8_t *ret;
    char *path = fs->str_printf("%r" SEP_S "unicode" SEP_S "%s", fs->fox_home, name);

    ret = (uint8_t*)fs->read_from_file(size, path, &fg->st_mem);
    free(path);

    return ret;
}

static void add_composition(uint32_t c1, uint32_t c2, uint32_t ch)
{
    int hash = COMPO_HASH(c1, c2);
    CompoHash *d = fs->Mem_get(&fg->st_mem, sizeof(CompoHash));

    d->c1 = c1;
    d->c2 = c2;
    d->ch = ch;
    d->next = compo_hash[hash];
    compo_hash[hash] = d;
}
static uint32_t get_composition(uint32_t c1, uint32_t c2)
{
    int hash = COMPO_HASH(c1, c2);
    CompoHash *d = compo_hash[hash];

    while (d != NULL) {
        if (d->c1 == c1 && d->c2 == c2) {
            return d->ch;
        }
        d = d->next;
    }
    return 0;
}
static void load_composition_data(void)
{
    int size;
    uint8_t *data;
    char *path = fs->str_printf("%r" SEP_S "unicode" SEP_S "composition", fs->fox_home);

    data = (uint8_t*)fs->read_from_file(&size, path, NULL);
    free(path);

    if (data != NULL) {
        const uint8_t *end = data + size;
        uint8_t *p = data;

        while (p + sizeof(uint32_t) * 3 <= end) {
            uint32_t c1, c2, ch;

            c1 = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
            p += sizeof(uint32_t);
            c2 = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
            p += sizeof(uint32_t);
            ch = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
            p += sizeof(uint32_t);
            add_composition(c1, c2, ch);
        }
        free(data);
    }
}

static int ch_get_property(uint8_t **data, const char *type, int code)
{
    if (code >= 0 && code < CODEPOINT_END) {
        int plane = code / CODEPOINTS_PLANE;
        uint8_t *ch;

        if (data[plane] == NULL) {
            data[plane] = load_unicode_property(type, plane);
        }
        ch = data[plane];
        if (ch == PTR_NO_DATA) {
            return 0;
        } else {
            int i = code % CODEPOINTS_PLANE;
            return ch[i];
        }
    } else {
        return 0;
    }
}
static int ch_get_unicode16(uint8_t **data, const char *type, int code)
{
    if (code >= 0 && code < CODEPOINT_END) {
        int plane = code / CODEPOINTS_PLANE;
        uint8_t *ch;

        if (data[plane] == NULL) {
            data[plane] = load_unicode_property(type, plane);
        }
        ch = data[plane];
        if (ch == PTR_NO_DATA) {
            return 0;
        } else {
            int i = (code % CODEPOINTS_PLANE * 2);
            return (ch[i] << 8) | ch[i + 1];
        }
    } else {
        return 0;
    }
}
static int ch_get_category(int code)
{
    return ch_get_property(ch_category, "ct", code);
}

static void StrBuf_add_i32(StrBuf *s, uint32_t i)
{
    uint32_t *dst;

    if (s->alloc_size <= s->size + 4) {
        s->alloc_size *= 2;
        s->p = realloc(s->p, s->alloc_size);
    }
    dst = (uint32_t*)&s->p[s->size];
    *dst = i;
    s->size += 4;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * TRUE  : 文字列を分割して配列を返す
 * FALSE : 分割した数を返す
 */
static int breaks_split(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(v[1]);
    int type = name_to_break_type(Value_vp(v[2]));
    const char *p = src->c;
    const char *end = p + src->size;
    int cnt = 0;
    RefArray *arr;

    if (FUNC_INT(node)) {
        arr = fs->refarray_new(0);
        *vret = vp_Value(arr);
    } else {
        arr = NULL;
    }

    switch (type) {
    case BREAKS_POINT: {
        while (p < end) {
            const char *top = p;
            fs->utf8_next(&p, end);
            if (arr != NULL) {
                refarray_push_str(arr, top, p - top, fs->cls_str);
            } else {
                cnt++;
            }
        }
        break;
    }
    case BREAKS_CHAR: {
        int prev = GRAPHEME_Other;
        const char *top = p;

        while (p < end) {
            int gr = ch_get_property(ch_grapheme, "gr", fs->utf8_codepoint_at(p));
            if ((grapheme_break[prev] & (1 << gr)) == 0) {
                // 分割できる
                if (arr != NULL) {
                    if (top < p) {
                        refarray_push_str(arr, top, p - top, fs->cls_str);
                    }
                    top = p;
                } else {
                    cnt++;
                }
            }

            prev = gr;
            fs->utf8_next(&p, end);
        }
        if (arr != NULL && top < p) {
            refarray_push_str(arr, top, p - top, fs->cls_str);
        }
        break;
    }
    case BREAKS_WORD:
        break;
    case BREAKS_SENTENCE:
        break;
    case BREAKS_LINE: {
        int prev = '\0';
        const char *top = p;

        while (p < end) {
            int c = fs->utf8_codepoint_at(p);
            switch (c) {
            case '\n':
                if (prev != '\r') {
                    // \r\nは1と数える
                    if (arr != NULL) {
                        refarray_push_str(arr, top, p - top, fs->cls_str);
                        top = p + 1;
                    } else {
                        cnt++;
                    }
                } else {
                    top++;
                }
                break;
            case '\r':
            case 0x0B:   // U+000B LINE TABULATION
            case 0x0C:   // U+000C FORM FEED (FF)
            case 0x85:   // U+0085 NEXT LINE (NEL)
            case 0x2028: // U+2028 LINE SEPARATOR
            case 0x2029: // U+2029 PARAGRAPH SEPARATOR
                if (arr != NULL) {
                    refarray_push_str(arr, top, p - top, fs->cls_str);
                    top = p + 1;
                } else {
                    cnt++;
                }
                break;
            }
            prev = c;
            fs->utf8_next(&p, end);
        }
        // 最後が改行で終わっていない場合+1
        if (prev != '\r' && prev != '\n') {
            if (arr != NULL) {
                refarray_push_str(arr, top, p - top, fs->cls_str);
            } else {
                cnt++;
            }
        }
        break;
    }
    default:
        return FALSE;
    }
    if (arr == NULL) {
        *vret = int32_Value(cnt);
    }
    return TRUE;
}

static int breakiter_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r;
    RefNode *b_iter = FUNC_VP(node);
    int type = name_to_break_type(Value_vp(v[2]));

    if (type == BREAKS_NONE) {
        return FALSE;
    }
    r = fs->ref_new(b_iter);
    *vret = vp_Value(r);

    r->v[BREAKITER_SRC] = fs->Value_cp(v[1]);
    r->v[BREAKITER_TYPE] = int32_Value(type);
    if (fg->stk_top > v + 3) {
        r->v[BREAKITER_LOCALE] = fs->Value_cp(v[3]);
    }

    return TRUE;
}
static int breakiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int src_begin = Value_integral(r->v[BREAKITER_BEGIN]);
    int src_end = Value_integral(r->v[BREAKITER_END]);
    RefStr *src = Value_vp(r->v[BREAKITER_SRC]);
    int ret = FALSE;

    switch (Value_integral(r->v[BREAKITER_TYPE])) {
    case BREAKS_POINT: {
        const char *p = src->c + src_end;
        const char *end = src->c + src->size;
        src_begin = src_end;
        fs->utf8_next(&p, end);
        src_end = p - src->c;
        
        ret = (src_begin < src_end);
        break;
    }
    case BREAKS_CHAR: {
        const char *p = src->c + src_end;
        const char *end = src->c + src->size;
        src_begin = src_end;

        if (p < end) {
            int prev = ch_get_property(ch_grapheme, "gr", fs->utf8_codepoint_at(p));
            fs->utf8_next(&p, end);

            while (p < end) {
                int gr = ch_get_property(ch_grapheme, "gr", fs->utf8_codepoint_at(p));
                if ((grapheme_break[prev] & (1 << gr)) == 0) {
                    // 分割できる
                    break;
                }
                prev = gr;
                fs->utf8_next(&p, end);
            }
            ret = TRUE;
        }
        src_end = p - src->c;
        break;
    }
    case BREAKS_WORD:
        break;
    case BREAKS_SENTENCE:
        break;
    case BREAKS_LINE:
        break;
    }
    r->v[BREAKITER_BEGIN] = int32_Value(src_begin);
    r->v[BREAKITER_END] = int32_Value(src_end);

    if (ret) {
        *vret = fs->cstr_Value(fs->cls_str, src->c + src_begin, src_end - src_begin);
    } else {
        fs->throw_stopiter();
        return FALSE;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////

static int unicode_category(Value *vret, Value *v, RefNode *node)
{
    static const char type_name[] =
            // その他
            "Cn" "Cc" "Cf" "Co" "Cs"
            // 文字
            "Ll" "Lm" "Lo" "Lt" "Lu"
            // 結合文字
            "Mc" "Me" "Mn"
            // 数字
            "Nd" "Nl" "No"
            // 句読点
            "Pc" "Pd" "Pe" "Pf" "Pi" "Po" "Ps"
            // 記号
            "Sc" "Sk" "Sm" "So"
            // 分離子
            "Zl" "Zp" "Zs";

    RefStr *src = Value_vp(v[1]);

    if (src->size > 0) {
        int type = ch_get_category(fs->utf8_codepoint_at(src->c));
        *vret = fs->cstr_Value(fs->cls_str, &type_name[type * 2], 2);
    } else {
        fs->throw_errorf(fs->mod_lang, "ValueError", "string length == 0");
        return FALSE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////

static int utf8_to_utf32(uint32_t *dst, const char *src, int size)
{
    int i = 0;
    const char *end = src + size;

    while (src < end) {
        int c = fs->utf8_codepoint_at(src);
        dst[i++] = c;
        fs->utf8_next(&src, end);
    }
    return i;
}
static int strlen_utf32_as_utf8(const uint32_t *src, int size)
{
    int i;
    int len = 0;

    for (i = 0; i < size; i++) {
        int ch = src[i];

        if (ch < 0x80) {
            len += 1;
        } else if (ch < 0x800) {
            len += 2;
        } else if (ch < 0x10000) {
            len += 3;
        } else {
            len += 4;
        }
    }
    return len;
}
static void utf32_to_utf8(char *dst, const uint32_t *src, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        int ch = src[i];

        if (ch < 0x80) {
            *dst++ = ch;
        } else if (ch < 0x800) {
            *dst++ = 0xC0 | (ch >> 6);
            *dst++ = 0x80 | (ch & 0x3F);
        } else if (ch < 0x10000) {
            *dst++ = 0xE0 | (ch >> 12);
            *dst++ = 0x80 | ((ch >> 6) & 0x3F);
            *dst++ = 0x80 | (ch & 0x3F);
        } else if (ch < 0x110000) {
            *dst++ = 0xF0 | (ch >> 18);
            *dst++ = 0x80 | ((ch >> 12) & 0x3F);
            *dst++ = 0x80 | ((ch >> 6) & 0x3F);
            *dst++ = 0x80 | (ch & 0x3F);
        }
    }
}
/**
 * 1文字を正規分解した結果を取得
 */
static const uint32_t *char_decomposition(uint8_t **data, const char *name, int code)
{
    int offset = ch_get_unicode16(data, name, code);
    if (offset != 0) {
        return &code_normalize[offset];
    } else {
        return NULL;
    }
}
/**
 * ハングル分解
 */
static const uint32_t *hangul_decomposition(int code)
{
    static uint32_t c[4];

    int s_index = code - HANGUL_S_BASE;
    if (s_index >= 0 && s_index < HANGUL_S_COUNT) {
        int t_index;

        c[0] = HANGUL_L_BASE + s_index / HANGUL_N_COUNT;
        c[1] = HANGUL_V_BASE + (s_index % HANGUL_N_COUNT) / HANGUL_T_COUNT;
        t_index = s_index % HANGUL_T_COUNT;
        if (t_index != 0) {
            c[2] = HANGUL_T_BASE + t_index;
            c[3] = 0;
        } else {
            c[2] = 0;
        }
        return c;
    }
    return NULL;
}

/**
 * 正規分解・互換分解
 */
static void unicode_decomposition_z(StrBuf *sb, const uint32_t *src, uint8_t **data, const char *name)
{
    int i;

    for (i = 0; src[i] != 0; i++) {
        const uint32_t *d = char_decomposition(data, name, src[i]);
        if (d != NULL) {
            unicode_decomposition_z(sb, d, data, name);
        } else {
            StrBuf_add_i32(sb, src[i]);
        }
    }
}
static void unicode_decomposition(StrBuf *sb, const uint32_t *src, int size, uint8_t **data, const char *name)
{
    int i;

    for (i = 0; i < size; i++) {
        int ch = src[i];
        const uint32_t *d = char_decomposition(data, name, ch);
        if (d != NULL) {
            unicode_decomposition_z(sb, d, data, name);
        } else {
            d = hangul_decomposition(ch);
            if (d != NULL) {
                while (*d != 0) {
                    StrBuf_add_i32(sb, *d);
                    d++;
                }
            } else {
                StrBuf_add_i32(sb, ch);
            }
        }
    }
}
/**
 * 正規順序で並べ替える
 */
static void sort_combi_class_order(uint32_t *src, int size)
{
    enum {
        MAX_LEN = 16,
    };
    uint8_t *ord;
    uint8_t a_ord[MAX_LEN];
    uint8_t *b_ord;
    int i, j;

    if (size > MAX_LEN) {
        b_ord = malloc(size);
        ord = b_ord;
    } else {
        b_ord = NULL;
        ord = a_ord;
    }

    for (i = 0; i < size; i++) {
        ord[i] = ch_get_property(ch_combi_class, "cc", src[i]);
    }
    // ordを昇順で並べ替え(安定ソート)
    for (i = 1; i < size; i++) {
        for (j = i; j > 0; j--) {
            uint32_t s;
            int o;
            if (ord[j - 1] <= ord[j]) {
                break;
            }
            o = ord[j];
            ord[j] = ord[j - 1];
            ord[j - 1] = o;
            s = src[j];
            src[j] = src[j - 1];
            src[j - 1] = s;
        }
    }

    if (b_ord != NULL) {
        free(b_ord);
    }
}
static void unicode_combi_class_order(uint32_t *src, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        if (ch_get_property(ch_combi_class, "cc", src[i]) != 0) {
            int begin = i;
            i++;
            for (; i < size; i++) {
                if (ch_get_property(ch_combi_class, "cc", src[i]) == 0) {
                    break;
                }
            }
            if (i - begin > 1) {
                sort_combi_class_order(&src[begin], i - begin);
            }
        }
    }
}

/**
 * 正準合成
 */
static int unicode_composition(uint32_t *buffer, int size)
{
    int i;
    int dst = 0;
    int max_cc = 0;
    uint32_t *starter = NULL;

    for (i = 0; i < size; i++) {
        uint32_t cmp;
        int ch = buffer[i];
        int cc = ch_get_property(ch_combi_class, "cc", ch);

        if (starter != NULL && max_cc < cc) {
            int l_index = *starter - HANGUL_L_BASE;
            int s_index = *starter - HANGUL_S_BASE;

            if (l_index >= 0 && l_index < HANGUL_L_COUNT) {
                int v_index = ch - HANGUL_V_BASE;
                if (v_index >= 0 && v_index < HANGUL_V_COUNT) {
                    *starter = HANGUL_S_BASE + (l_index * HANGUL_V_COUNT + v_index) * HANGUL_T_COUNT;
                    continue;
                }
            }
            if (s_index >= 0 && s_index < HANGUL_S_COUNT && s_index % HANGUL_T_COUNT == 0) {
                int t_index = ch - HANGUL_T_BASE;
                if (t_index >= 0 && t_index < HANGUL_T_COUNT) {
                    *starter += t_index;
                    continue;
                }
            }
        }
        if (starter != NULL && max_cc < cc && ((cmp = get_composition(*starter, ch)) != 0)) {
            // 結合済み文字
            *starter = cmp;
            cc = ch_get_property(ch_combi_class, "cc", cmp);
            if (cc == 0) {
                max_cc = -1;
            } else if (max_cc < cc) {
                max_cc = cc;
            }
        } else {
            if (cc == 0) {
                max_cc = -1;
                starter = &buffer[dst];
            } else if (max_cc < cc) {
                max_cc = cc;
            }
            buffer[dst++] = ch;
        }
    }
    return dst;
}

static int unicode_normalize(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(v[1]);
    RefStr *mode = Value_vp(v[2]);
    uint32_t *cbuf;
    int cbuf_size;
    StrBuf sb;
    uint8_t **ch_normalize;
    const char *norm_name;
    int composition = FALSE;
    int srclen;
    int case_fold = FALSE;

    if (code_normalize == NULL) {
        int size = 0, i;
        uint8_t *p = load_unicode_data(&size, "normalize");
        code_normalize = (uint32_t*)p;

        // エンディアンをマシンローカルに修正
        size /= sizeof(uint32_t);
        for (i = 0; i < size; i++) {
            int j = i * sizeof(uint32_t);
            uint32_t tmp = (p[j] << 24) | (p[j + 1] << 16) | (p[j + 2] << 8) | p[j + 3];
            code_normalize[i] = tmp;
        }
    }

    srclen = strlen_utf8(src->c, src->size);
    cbuf = malloc(srclen * sizeof(uint32_t));
    cbuf_size = utf8_to_utf32(cbuf, src->c, src->size);
    fs->StrBuf_init(&sb, srclen + 1);

    if (str_eq(mode->c, mode->size, "D", 1)) {
        // 正準分解
        ch_normalize = ch_normalize_c;
        norm_name = "nc";
    } else if (str_eq(mode->c, mode->size, "C", 1)) {
        // 正準分解・正準合成
        ch_normalize = ch_normalize_c;
        norm_name = "nc";
        composition = TRUE;
    } else if (str_eq(mode->c, mode->size, "KD", 2)) {
        // 互換分解
        ch_normalize = ch_normalize_k;
        norm_name = "nk";
    } else if (str_eq(mode->c, mode->size, "KC", 2)) {
        // 互換分解・正準合成
        ch_normalize = ch_normalize_k;
        norm_name = "nk";
        composition = TRUE;
    } else if (str_eq(mode->c, mode->size, "KC_CF", 5)) {
        // 互換分解・正準合成・Casefold
        ch_normalize = ch_normalize_k;
        norm_name = "nk";
        composition = TRUE;
        case_fold = TRUE;
    } else {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown option %Q", mode);
        free(cbuf);
        return FALSE;
    }

    unicode_decomposition(&sb, cbuf, cbuf_size, ch_normalize, norm_name);
    free(cbuf);
    cbuf = (uint32_t*)sb.p;
    cbuf_size = sb.size / sizeof(uint32_t);
    unicode_combi_class_order(cbuf, cbuf_size);

    if (composition) {
        // 正準合成
        if (!compo_hash_loaded) {
            load_composition_data();
            compo_hash_loaded = TRUE;
        }
        cbuf_size = unicode_composition(cbuf, cbuf_size);
    }
    if (case_fold) {
        // NFKC_Casefold
    }

    {
        RefStr *rs = fs->refstr_new_n(fs->cls_str, strlen_utf32_as_utf8(cbuf, cbuf_size));
        *vret = vp_Value(rs);
        utf32_to_utf8(rs->c, cbuf, cbuf_size);
        rs->c[rs->size] = '\0';
    }
    StrBuf_close(&sb);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "breaks_count", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, breaks_split, 2, 3, (void*) FALSE, fs->cls_str, fs->cls_str, fs->cls_locale);

    n = fs->define_identifier(m, m, "breaks_split", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, breaks_split, 2, 3, (void*) TRUE, fs->cls_str, fs->cls_str, fs->cls_locale);

    n = fs->define_identifier(m, m, "char_category", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, unicode_category, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier(m, m, "normalize", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, unicode_normalize, 2, 2, NULL, fs->cls_str, fs->cls_str);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    // BreakIterator
    cls = fs->define_identifier(m, m, "BreakIterator", NODE_CLASS, 0);
    cls->u.c.n_memb = BREAKITER_NUM;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, breakiter_new, 2, 3, cls, fs->cls_str, fs->cls_str, fs->cls_locale);

    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, breakiter_next, 0, 0, NULL);

    fs->extends_method(cls, fs->cls_iterator);
}

static UnicodeStatic *UnicodeStatic_new(void)
{
    UnicodeStatic *f = fs->Mem_get(&fg->st_mem, sizeof(UnicodeStatic));

    f->ch_get_category = ch_get_category;

    return f;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_unicode = m;

    m->u.m.ext = UnicodeStatic_new();

    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;
    
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }

    if (buf == NULL) {
        char *path;
        char *version;

        fs = a_fs;
        path = fs->str_printf("%r" SEP_S "unicode" SEP_S "version.txt", fs->fox_home);
        version = fs->read_from_file(NULL, path, NULL);

        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\nUnicode\t%.32s\n", version);
        free(path);
        free(version);
    }
    return buf;
}
