#include "fconv.h"
#include "m_codecvt.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum {
    CODESET_NONE,

    CODESET_256,
    CODESET_96_96,
    CODESET_96_192,
};

enum {
    USER_DEFINED_BEGIN = 0xE000,
    USER_DEFINED_END = 0xF900,
};

typedef union {
    int32_t ip;
    const char *p;
} FToU8;

typedef struct {
    int32_t type;
    int32_t size;
    FToU8 u[];
} FToU8Table;

typedef struct FFromU8 {
    int32_t code;
    FToU8Table *tbl;
    struct FFromU8 **next;
} FFromU8;

typedef struct {
    FFromU8 *next[256];
} FFromU8Table;

typedef struct {
    uint8_t sign;
} FEscape;

struct FCharset {
    RefCharset *rc;
    int type;
    FFromU8Table *f;
    FToU8Table **t;
    int esc_num;
    FEscape *esc;
};

static int32_t fread_int32(FileHandle fh)
{
    char buf[4];
    if (read_fox(fh, buf, 4) == 4) {
        return (int32_t)ptr_read_uint32(buf);
    } else {
        return 0;
    }
}

static FToU8Table *load_U8Table(const char *fname, Mem *mem)
{
    FileHandle fh = open_fox(fname, O_RDONLY, DEFAULT_PERMISSION);
    if (fh != -1) {
        FToU8Table *table;
        int size, i;
        char *str;
        int32_t type = fread_int32(fh);
        switch (type) {
        case CODESET_256:
            size = 256;
            break;
        case CODESET_96_96:
            size = 96 * 96;
            break;
        case CODESET_96_192:
            size = 96 * 192;
            break;
        default:
            close_fox(fh);
            return NULL;
        }

        table = (FToU8Table*)fs->Mem_get(mem, sizeof(FToU8Table) + (size + 1) * sizeof(FToU8));
        table->type = type;
        table->size = size;

        for (i = 0; i < size; i++) {
            table->u[i].ip = fread_int32(fh);
        }
        {
            int str_size = fread_int32(fh);
            str = (char*)fs->Mem_get(mem, str_size);
            read_fox(fh, str, str_size);
        }
        for (i = 0; i < size; i++) {
            int32_t ip = table->u[i].ip;
            if (ip == -1) {
                table->u[i].p = NULL;
            } else {
                table->u[i].p = &str[ip];
            }
        }
        table->u[i].p = NULL;
        close_fox(fh);
        return table;
    } else {
        fs->fatal_errorf("Cannot read file %q", fname);
        return NULL;
    }
}
static void appendU8Sub(FFromU8 *from, FToU8Table *ttable, const char *str, int32_t code, Mem *mem)
{
    for (;;) {
        if (*str == '\0') {
            if (from->tbl == NULL) {
                from->code = code;
                from->tbl = ttable;
            }
            return;
        }
        if (from->next == NULL) {
            FFromU8 **pp = (FFromU8**)fs->Mem_get(mem, sizeof(FFromU8*) * 64);
            memset(pp, 0, sizeof(FFromU8*) * 64);
            from->next = pp;
        }
        {
            int c = *str & 0x3F;
            FFromU8 *u8 = from->next[c];
            if (u8 == NULL) {
                u8 = (FFromU8*)fs->Mem_get(mem, sizeof(FFromU8));
                memset(u8, 0, sizeof(FFromU8));
                from->next[c] = u8;
            }
            from = u8;
            str++;
        }
    }
}
static void appendU8Table(FFromU8Table *from, FToU8Table *to, Mem *mem)
{
    int i;
    for (i = 0; i < to->size; i++) {
        const char *str = to->u[i].p;
        if (str != NULL) {
            int c = *str & 0xFF;
            FFromU8 *u8 = from->next[c];
            int32_t code = 0;
            if (u8 == NULL) {
                u8 = (FFromU8*)fs->Mem_get(mem, sizeof(FFromU8));
                memset(u8, 0, sizeof(FFromU8));
                from->next[c] = u8;
            }
            switch (to->type) {
            case CODESET_256:
                code = i;
                break;
            case CODESET_96_96: {
                int c1 = i / 96;
                int c2 = i % 96;
                code = (c1 << 8) | c2;
                break;
            }
            case CODESET_96_192: {
                int c1 = i / 192;
                int c2 = i % 192;
                code = (c1 << 8) | c2;
                break;
            }
            }
            appendU8Sub(u8, to, str + 1, code, mem);
        }
    }
}
static FFromU8 *findFromU8(FFromU8Table *from, const char **psrc)
{
    const char *src = *psrc;
    FFromU8 *u8 = from->next[*src & 0xFF];
    FFromU8 *last = NULL;
    src++;
    if (u8 == NULL) {
        return NULL;
    }
    if (u8->tbl != NULL) {
        last = u8;
    }
    for (;;) {
        FFromU8 *next;
        if (u8->tbl != NULL) {
            last = u8;
        }
        if (u8->next == NULL || (next = u8->next[*src & 0x3F]) == NULL) {
            break;
        }
        src++;
        u8 = next;
        if (u8->tbl != NULL) {
            last = u8;
        }
    }
    if (last != NULL) {
        *psrc = src;
    }
    return last;
}

/////////////////////////////////////////////////////////////////////////

FCharset *FCharset_new(RefCharset *rc)
{
    Mem mem;
    FCharset *cs;

    fs->Mem_init(&mem, 1024);
    cs = (FCharset*)fs->Mem_get(&fg->st_mem, sizeof(FCharset));
    cs->rc = rc;
    cs->type = rc->type;

    if (rc->type >= FCHARSET_SINGLE_BYTE && rc->files != NULL) {
        int i;
        int num_t = 1;

        switch (rc->type) {
        case FCHARSET_SINGLE_BYTE:
            num_t = 2;
            break;
        case FCHARSET_EUC:
        case FCHARSET_SHIFTJIS:
            num_t = 4;
            break;
        }

        cs->f = (FFromU8Table*)fs->Mem_get(&mem, sizeof(FFromU8Table));
        cs->t = (FToU8Table**)fs->Mem_get(&mem, sizeof(FToU8Table*) * num_t);
        memset(cs->f, 0, sizeof(FFromU8Table));
        memset(cs->t, 0, sizeof(FToU8Table) * num_t);

        for (i = 0; i < num_t && rc->files[i] != NULL; i++) {
            const char *file = rc->files[i];
            cs->t[i] = load_U8Table(file, &mem);
            appendU8Table(cs->f, cs->t[i], &mem);
        }
    } else {
        cs->f = NULL;
        cs->t = NULL;
    }
    return cs;
}

// 不正なUTF-8を入力した場合の動作は未定義
// 出力先は最低FCONV_MAX_CHAR_LENGTH確保されている
int FCharset_from_utf8(FCharset *cs, const char **psrc, char **pdst)
{
    const uint8_t *src = (const uint8_t*)*psrc;
    uint8_t *dst = (uint8_t*)*pdst;

    switch (cs->type) {
    case FCHARSET_ASCII:
        if ((*src & 0x80) == 0) {
            *dst = *src;
            *psrc = (const char*)src + 1;
            *pdst = (char*)dst + 1;
            return TRUE;
        }
        break;
    case FCHARSET_UTF8:
    case FCHARSET_UTF8_LOOSE: {
        // そのまま
        int len;
        int c = *src;
        if ((c & 0x80) == 0) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len =  2;
        } else if ((c & 0xF0) == 0xE0) {
            len =  3;
        } else {
            len =  4;
        }
        memcpy(dst, src, len);
        *psrc = (const char*)src + len;
        *pdst = (char*)dst + len;
        return TRUE;
    }
    case FCHARSET_UTF16LE:
    case FCHARSET_UTF16BE:
    case FCHARSET_UTF32LE:
    case FCHARSET_UTF32BE:
    case FCHARSET_CESU8: {
        int src_len = 0;
        int code = -1;
        int c = *src;

        if ((c & 0x80) == 0) {
            code = c & 0x7F;
            src_len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            code = ((c & 0x1F) << 6) | (src[1] & 0x3F);
            src_len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            code = ((c & 0x0F) << 12) | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F);
            src_len = 3;
        } else {
            code = ((c & 0x07) << 18) | ((src[1] & 0x3F) << 12)  | ((src[2] & 0x3F) << 6) | (src[3] & 0x3F);
            src_len = 4;
        }

        switch (cs->type) {
        case FCHARSET_UTF16LE:
            if (code < 0x10000) {
                *dst++ = code & 0xFF;
                *dst++ = (code >> 8) & 0xFF;
            } else {
                int su = ((code - 0x10000) >> 10) | SURROGATE_U_BEGIN;
                int sl = ((code - 0x10000) & 0x3FF) | SURROGATE_L_BEGIN;
                *dst++ = su & 0xFF;
                *dst++ = (su >> 8) & 0xFF;
                *dst++ = sl & 0xFF;
                *dst++ = (sl >> 8) & 0xFF;
            }
            break;
        case FCHARSET_UTF16BE:
            if (code < 0x10000) {
                *dst++ = (code >> 8) & 0xFF;
                *dst++ = code & 0xFF;
            } else {
                int su = ((code - 0x10000) >> 10) | SURROGATE_U_BEGIN;
                int sl = ((code - 0x10000) & 0x3FF) | SURROGATE_L_BEGIN;
                *dst++ = (su >> 8) & 0xFF;
                *dst++ = su & 0xFF;
                *dst++ = (sl >> 8) & 0xFF;
                *dst++ = sl & 0xFF;
            }
            break;
        case FCHARSET_UTF32LE:
            *dst++ = code & 0xFF;
            *dst++ = (code >> 8) & 0xFF;
            *dst++ = (code >> 16) & 0xFF;
            *dst++ = 0;
            break;
        case FCHARSET_UTF32BE:
            *dst++ = 0;
            *dst++ = (code >> 16) & 0xFF;
            *dst++ = (code >> 8) & 0xFF;
            *dst++ = code & 0xFF;
            break;
        case FCHARSET_CESU8:
            if (code < 0x80) {
                *dst++ = code;
            } else if (code < 0x800) {
                *dst++ = 0xC0 | (code >> 6);
                *dst++ = 0x80 | (code & 0x3F);
            } else if (code < 0x10000) {
                *dst++ = 0xE0 | (code >> 12);
                *dst++ = 0x80 | ((code >> 6) & 0x3F);
                *dst++ = 0x80 | (code & 0x3F);
            } else {
                int su = ((code - 0x10000) >> 10) | SURROGATE_U_BEGIN;
                int sl = ((code - 0x10000) & 0x3FF) | SURROGATE_L_BEGIN;
                *dst++ = 0xE0 | (su >> 12);
                *dst++ = 0x80 | ((su >> 6) & 0x3F);
                *dst++ = 0x80 | (su & 0x3F);
                *dst++ = 0xE0 | (sl >> 12);
                *dst++ = 0x80 | ((sl >> 6) & 0x3F);
                *dst++ = 0x80 | (sl & 0x3F);
            }
            break;
        }
        *psrc = (const char*)src + src_len;
        *pdst = (char*)dst;
        return TRUE;
    }
    default: {
        FFromU8 *ftable = findFromU8(cs->f, (const char**)&src);
        if (ftable != NULL) {
            switch (cs->type) {
            case FCHARSET_SINGLE_BYTE:
                *dst++ = (char)ftable->code;
                *psrc = (const char*)src;
                *pdst = (char*)dst;
                return TRUE;
            case FCHARSET_SHIFTJIS:
                if (ftable->tbl == cs->t[2]) {
                    int row = (ftable->code >> 8) & 0xFF;
                    int col = ftable->code & 0xFF;
                    if (row < 63) {
                        *dst++ = (row + 0x101) / 2;
                    } else {
                        *dst++ = (row + 0x181) / 2;
                    }
                    if (row % 2 == 0) {
                        *dst++ = col + 0x9e;
                    } else if (col < 64) {
                        *dst++ = col + 0x3f;
                    } else {
                        *dst++ = col + 0x40;
                    }
                } else if (ftable->tbl == cs->t[3]) {
                    int row = (ftable->code >> 8) & 0xFF;
                    int col = ftable->code & 0xFF;
                    *dst++ = row / 2 + 0xE0;
                    if (row % 2 == 0) {
                        *dst++ = col + 0x9E;
                    } else if (col < 64) {
                        *dst++ = col + 0x3F;
                    } else {
                        *dst++ = col + 0x40;
                    }
                } else {
                    *dst++ = (char)ftable->code;
                }
                *psrc = (const char*)src;
                *pdst = (char*)dst;
                return TRUE;
            case FCHARSET_BIG5:
                if (ftable->tbl->type == CODESET_96_192) {
                    int row = (ftable->code >> 8) & 0xFF;
                    int col = ftable->code & 0xFF;
                    *dst++ = row + 0xA0;
                    *dst++ = col + 0x40;
                } else {
                    *dst++ = (char)ftable->code;
                }
                return TRUE;
            case FCHARSET_EUC:
                if (ftable->tbl == cs->t[0]) {
                    *dst++ = ftable->code;
                } else if (ftable->tbl == cs->t[1]) {
                    *dst++ = (ftable->code >> 8) + 0xA0;
                    *dst++ = (ftable->code & 0xFF) + 0xA0;
                } else if (ftable->tbl == cs->t[2]) {
                    *dst++ = 0x8E;
                    *dst++ = (ftable->code >> 8) + 0xA0;
                    *dst++ = (ftable->code & 0xFF) + 0xA0;
                } else if (ftable->tbl == cs->t[3]) {
                    *dst++ = 0x8F;
                    *dst++ = (ftable->code >> 8) + 0xA0;
                    *dst++ = (ftable->code & 0xFF) + 0xA0;
                }
                *psrc = (const char*)src;
                *pdst = (char*)dst;
                return TRUE;
            default:
                break;
            }
        } else if ((*src & 0xF0) == 0xE0) {
            int code = ((src[0] & 0x0F) << 12) | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F);
            if (code >= USER_DEFINED_BEGIN && code < USER_DEFINED_END) {
                // User-defined range
                code -= USER_DEFINED_BEGIN;
                switch (cs->type) {
                case FCHARSET_SHIFTJIS:
                    if (cs->t[2] == NULL) {
                        int c1 = code / 188;
                        int c2 = code % 188;
                        *dst++ = 0xF0 + c1;  // F0 - F9
                        *dst++ = (c2 < 0x3F ? c2 + 0x40 : c2+0x41);
                        *psrc = (const char*)src + 3;
                        *pdst = (char*)dst;
                    }
                    break;
                case FCHARSET_EUC:
                    if (code < 940) {
                        *dst++ = 0xF5 + (code / 94);
                        *dst++ = 0xA1 + (code % 94);
                    } else {
                        *dst++ = 0x8F;
                        *dst++ = 0xF5 + ((code - 940) / 94);
                        *dst++ = 0xA1 + ((code - 940) % 94);
                    }
                    break;
                }
            }
        }
        break;
    }
    }
    return FALSE;
}
static void output_utf8_3bytes(uint8_t *dst, int code)
{
    *dst++ = 0xE0 | (code >> 12);
    *dst++ = 0x80 | ((code >> 6) & 0x3F);
    *dst++ = 0x80 | (code & 0x3F);
}
// 出力先は最低FCONV_MAX_CHAR_LENGTH確保されている
int FCharset_to_utf8(FCharset *cs, const char **psrc, const char *src_end, char **pdst)
{
    const uint8_t *src = (const uint8_t*)*psrc;
    uint8_t *dst = (uint8_t*)*pdst;

    switch (cs->type) {
    case FCHARSET_ASCII:
        if ((*src & 0x80) == 0) {
            *dst = *src;
            *psrc = (const char*)src + 1;
            *pdst = (char*)dst + 1;
            return TRUE;
        }
        break;
    case FCHARSET_UTF8:
    case FCHARSET_UTF8_LOOSE:
    case FCHARSET_UTF16LE:
    case FCHARSET_UTF16BE:
    case FCHARSET_UTF32LE:
    case FCHARSET_UTF32BE: {
        int code = -1;
        int src_len = 0;

        switch (cs->type) {
        case FCHARSET_UTF8: {
            int c = *src;

            // 非最短型を除く
            if ((c & 0x80) == 0) {
                code = c;
                src_len = 1;
            } else if ((c & 0xE0) == 0xC0) {
                if (((const uint8_t*)src_end - src) >= 2) {
                    if ((c & 0xFF) >= 0xC2) {
                        src_len = 2;
                        code = ((c & 0x1F) << 6) | (src[1] & 0x3F);
                    }
                }
            } else if ((c & 0xF0) == 0xE0) {
                if (((const uint8_t*)src_end - src) >= 3) {
                    if (src[0] > 0xE0 || (src[0] == 0xE0 && (src[1] & 0xFF) >= 0xA0)) {
                        src_len = 3;
                        code = ((c & 0x0F) << 12) | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F);
                    }
                }
            } else if ((c & 0xF8) == 0xF0) {
                if (((const uint8_t*)src_end - src) >= 4) {
                    if (src[0] > 0xF0 || (src[0] == 0xF0 && (src[1] & 0xFF) >= 0x90)) {
                        src_len = 4;
                        code = ((c & 0x07) << 18) | ((src[1] & 0x3F) << 12)  | ((src[2] & 0x3F) << 6) | (src[3] & 0x3F);
                    }
                }
            }
            break;
        }
        case FCHARSET_UTF8_LOOSE:
        case FCHARSET_CESU8: {
            int c = *src;

            // 非最短型を許容
            if ((c & 0x80) == 0) {
                code = c;
                src_len = 1;
            } else if ((c & 0xE0) == 0xC0) {
                if (((const uint8_t*)src_end - src) >= 2) {
                    code = ((c & 0x1F) << 6) | (src[1] & 0x3F);
                    src_len = 2;
                }
            } else if ((c & 0xF0) == 0xE0) {
                if (((const uint8_t*)src_end - src) >= 3) {
                    code = ((c & 0x0F) << 12) | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F);
                    src_len = 3;
                }
            } else if ((c & 0xF8) == 0xF0) {
                if (((const uint8_t*)src_end - src) >= 4) {
                    code = ((c & 0x07) << 18) | ((src[1] & 0x3F) << 12)  | ((src[2] & 0x3F) << 6) | (src[3] & 0x3F);
                    src_len = 4;
                }
            }
            if (code >= SURROGATE_U_BEGIN && code < SURROGATE_L_BEGIN) {
                if (((const uint8_t*)src_end - src) >= 6 && (src[3] & 0xF0) == 0xE0) {
                    int code2 = ((src[3] & 0x0F) << 12) | ((src[4] & 0x3F) << 6) | (src[5] & 0x3F);
                    if (code2 >= SURROGATE_L_BEGIN && code2 < SURROGATE_END) {
                        code = (((code - SURROGATE_U_BEGIN) << 10) | (code2 - SURROGATE_L_BEGIN)) + 0x10000;
                        src_len = 6;
                    }
                }
            }
            break;
        }
        case FCHARSET_UTF16LE:
            if (src_len >= 2) {
                int c = src[0] | (src[1] << 8);
                if (c < SURROGATE_U_BEGIN) {
                    code = c;
                    src_len = 2;
                } else if (c < SURROGATE_L_BEGIN && src_len >= 4) {
                    int c2 = src[2] | (src[3] << 8);
                    if (c2 >= SURROGATE_L_BEGIN && c2 < SURROGATE_END) {
                        code = (((c - SURROGATE_U_BEGIN) << 10) | (c2 - SURROGATE_L_BEGIN)) + 0x10000;
                        src_len = 4;
                    }
                }
            }
            break;
        case FCHARSET_UTF16BE:
            if (src_len >= 2) {
                int c = src[1] | (src[0] << 8);
                if (c < SURROGATE_U_BEGIN) {
                    code = c;
                    src_len = 2;
                } else if (c < SURROGATE_L_BEGIN && src_len >= 4) {
                    int c2 = src[3] | (src[2] << 8);
                    if (c2 >= SURROGATE_L_BEGIN && c2 < SURROGATE_END) {
                        code = (((c - SURROGATE_U_BEGIN) << 10) | (c2 - SURROGATE_L_BEGIN)) + 0x10000;
                        src_len = 4;
                    }
                }
            }
            break;
        case FCHARSET_UTF32LE:
            if (src_len >= 4 && src[3] == 0) {
                int c = src[0] | (src[1] << 8) | (src[2] << 16);
                if (c < SURROGATE_U_BEGIN || (c >= SURROGATE_END && c < CODEPOINT_END)) {
                    code = c;
                    src_len = 4;
                }
            }
            break;
        case FCHARSET_UTF32BE:
            if (src_len >= 4 && src[0] == 0) {
                int c = src[3] | (src[2] << 8) | (src[1] << 16);
                if (c < SURROGATE_U_BEGIN || (c >= SURROGATE_END && c < CODEPOINT_END)) {
                    code = c;
                    src_len = 4;
                }
            }
            break;
        }
        if (code < 0) {
            return FALSE;
        } else if (code < 0x80) {
            *dst++ = code;
        } else if (code < 0x800) {
            *dst++ = 0xC0 | (code >> 6);
            *dst++ = 0x80 | (code & 0x3F);
        } else if (code < 0x10000) {
            *dst++ = 0xE0 | (code >> 12);
            *dst++ = 0x80 | ((code >> 6) & 0x3F);
            *dst++ = 0x80 | (code & 0x3F);
        } else {
            *dst++ = 0xF0 | (code >> 18);
            *dst++ = 0x80 | ((code >> 12) & 0x3F);
            *dst++ = 0x80 | ((code >> 6) & 0x3F);
            *dst++ = 0x80 | (code & 0x3F);
        }
        *psrc = (const char*)src + src_len;
        *pdst = (char*)dst;
        return TRUE;
    }
    default: {
        int c = *src & 0xFF;
        FToU8Table *u8 = NULL;
        int index = 0;
        if (c == '\0') {
            *dst++ = '\0';
            *psrc = (const char*)src + 1;
            *pdst = (char*)dst;
            return TRUE;
        }
        switch (cs->type) {
        case FCHARSET_SINGLE_BYTE:
            if (cs->t[1] != NULL && c >= 0x80) {
                u8 = cs->t[1];
            } else {
                u8 = cs->t[0];
            }
            index = c;
            src += 1;
            break;
        case FCHARSET_SHIFTJIS:
            if (c < 0x80) {
                // 00 - 7F
                u8 = cs->t[0];
                index = c;
                src += 1;
            } else if (c < 0x9F) {
                // 80 - 9F
                if (((const uint8_t*)src_end - src) >= 2) {
                    int c2 = src[1];
                    if (c2 >= 0x40 && c2 < 0xFF && c2 != 0x7F) {
                        index = (c - 0x81) * 192 + 96;
                        if (c2 >= 0x9F) {
                            index += (c2 - 0x9E) + 96;
                        } else if (c2 > 0x7F) {
                            index += c2 - 0x40;
                        } else {
                            index += c2 - 0x3F;
                        }
                        u8 = cs->t[2];
                        src += 2;
                    }
                }
            } else if (c < 0xA1) {
                // A0
            } else if (c < 0xE0) {
                // A1 - DF
                u8 = cs->t[1];
                index = c - 128;
                src += 1;
            } else if (c < 0xF0) {
                // E0 - EF
                if (((const uint8_t*)src_end - src) >= 2) {
                    int c2 = src[1];
                    if (c2 >= 0x3F && c2 != 0x7F) {
                        index = (c - 0xE0) * 192 + 63 * 96;
                        if (c2 >= 0x9F) {
                            index += (c2 - 0x9E) + 96;
                        } else if (c2 > 0x7F) {
                            index += c2 - 0x40;
                        } else {
                            index += c2 - 0x3F;
                        }
                        u8 = cs->t[2];
                        src += 2;
                    }
                }
            } else if (c < 0xFD) {
                // F0 - FE
                if (((const uint8_t*)src_end - src) >= 2) {
                    if (cs->t[3] != NULL) {
                        int c2 = src[1];
                        if (c2 >= 0x3F && c2 != 0x7F) {
                            index = (c - 0xF0) * 192;
                            if (c2 >= 0x9F) {
                                index += (c2 - 0x9E) + 96;
                            } else if (c2 > 0x7F) {
                                index += c2 - 0x40;
                            } else {
                                index += c2 - 0x3F;
                            }
                            u8 = cs->t[3];
                            src += 2;
                        }
                    } else if (c < 0xFA) {
                        // User-defined
                        int c2 = src[1];
                        if (c2 >= 0x3F && c2 < 0xFD && c2 != 0x7F) {
                            int code = c - 0xF0;
                            if (c2 > 0x7F) {
                                code += c2 - 0x40;
                            } else {
                                code += c2 - 0x3F;
                            }
                            output_utf8_3bytes(dst, code);
                            *psrc = (const char*)src + 2;
                            *pdst = (char*)dst + 3;
                            return TRUE;
                        }
                    }
                }
            }
            break;
        case FCHARSET_BIG5:
            if (c < 0x80) {
                // 00 - 7F
                u8 = cs->t[0];
                index = c;
                src += 1;
            } else if (c < 0xA1) {
                // User-defined range
            } else {
                // A1 - FF
                if (((const uint8_t*)src_end - src) >= 2) {
                    int c2 = src[1];
                    if (c2 >= 0x40 && c2 < 0xFF) {
                        u8 = cs->t[0];
                        index = (c - 0xA0) * 192 + (c2 - 0x40);
                        src += 2;
                    }
                }
            }
            break;
        case FCHARSET_EUC:
            if (c < 0x80) {
                u8 = cs->t[0];
            } else if (c >= 0xA1) {
                u8 = cs->t[1];
            } else if (c == 0x8E) {
                u8 = cs->t[2];
                src++;
            } else if (c == 0x8F) {
                u8 = cs->t[3];
                src++;
            }
            if (u8 != NULL) {
                if (u8->type == CODESET_96_96) {
                    if (((const uint8_t*)src_end - src) >= 2) {
                        int c1 = src[0];
                        int c2 = src[1];
                        if (c1 >= 0xA0 && c2 >= 0xA0) {
                            index = (c1 - 0xA0) * 96 + (c2 - 0xA0);
                            src += 2;
                        }
                    }
                } else {
                    if (((const uint8_t*)src_end - src) >= 1) {
                        index = *src & 0x7F;
                        src++;
                    }
                }
            }
            break;
        }
        if (u8 != NULL) {
            const char *d = u8->u[index].p;
            if (d != NULL) {
                while (*d != '\0') {
                    *dst++ = *d++;
                }
                *psrc = (const char*)src;
                *pdst = (char*)dst;
                return TRUE;
            }
        }
        break;
    }
    }
    return FALSE;
}

void FCharset_from_ascii_string(FCharset *cs, char *src, const char *end)
{
    if (cs->f != NULL && cs->type == CODESET_256) {
        FFromU8 **table = cs->f->next;
        uint8_t *p = (uint8_t*)src;
        while (p < (const uint8_t*)end) {
            FFromU8 *u8 = table[*p];
            if (u8->code > 0) {
                *p = u8->code;
            }
            p++;
        }
    }
}
void FConv_throw_charset_error(FConv *fc)
{
    if (fc->to_utf8) {
        fs->throw_errorf(fs->mod_lang, "CharsetError", "Cannot convert %02X to %S", fc->error_charcode, fc->cs->rc->name);
    } else {
        fs->throw_errorf(fs->mod_lang, "CharsetError", "Cannot convert %U to %S", fc->error_charcode, fc->cs->rc->name);
    }
}
