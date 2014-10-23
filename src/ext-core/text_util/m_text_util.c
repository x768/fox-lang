#include "fox.h"
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct MapRepl {
    struct MapRepl *next;
    Str key;
    Str val;
} MapRepl;

typedef struct {
    RefHeader rh;

    Mem mem;
    MapRepl *map[256];
} RefMapReplace;


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_util;
static RefNode *cls_replacer;


static char *init_mem_and_read_file(const char *path, Mem *mem)
{
    int fh = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
    int64_t size;
    char *buf;
    int init_size = 1024;
    if (fh == -1) {
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
        return NULL;
    }
    size = fs->get_file_size(fh);
    if (size == -1 || size > fs->max_alloc) {
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
        return NULL;
    }

    while (init_size * 4 < size) {
        init_size *= 2;
    }
    fs->Mem_init(mem, init_size);

    buf = fs->Mem_get(mem, size + 1);
    read_fox(fh, buf, size);
    buf[size] = '\0';
    close_fox(fh);

    return buf;
}
static void read_replacer_sub(RefMapReplace *rp, char *src)
{
    while (*src != '\0') {
        const char *top = src;
        while (*src != '\0' && *src != '\t' && *src != '\n') {
            src++;
        }
        if (*src == '\t') {
            Str key;
            Str val;

            key.p = top;
            key.size = src - top;
            src++;
            val.p = src;
            while (*src != '\0' && *src != '\n') {
                src++;
            }
            val.size = src - val.p;

            if (key.size > 0) {
                MapRepl **pp = &rp->map[key.p[0] & 0xFF];
                MapRepl *p = /* *pp;

                // keyが長い順に並べる
                while (p != NULL) {
                    if (p->key.size <= key.size) {
                        break;
                    }
                    pp = &p->next;
                    p = *pp;
                }
                p =*/ fs->Mem_get(&rp->mem, sizeof(MapRepl));
                p->next = *pp;
                p->key = key;
                p->val = val;
                *pp = p;
            }
        } else {
        }
        if (*src != '\0') {
            src++;
        }
    }
}

static int replacer_new(Value *vret, Value *v, RefNode *node)
{
    const RefNode *v1_type;

    if (fg->stk_top <= v + 1) {
        RefMapReplace *rp = fs->buf_new(cls_replacer, sizeof(RefMapReplace));
        *vret = vp_Value(rp);
        fs->Mem_init(&rp->mem, 1024);
        return TRUE;
    }

    v1_type = fs->Value_type(v[1]);
    if (v1_type == fs->cls_str) {
        // ファイルから読む
        Str path = fs->Value_str(v[1]);
        char *path_p = fs->resource_to_path(path, ".txt");
        char *data;
        RefMapReplace *rp;

        if (path_p == NULL) {
            return FALSE;
        }
        rp = fs->buf_new(cls_replacer, sizeof(RefMapReplace));
        *vret = vp_Value(rp);
        data = init_mem_and_read_file(path_p, &rp->mem);
        free(path_p);
        if (data == NULL) {
            return FALSE;
        }
        read_replacer_sub(rp, data);
    } else if (v1_type == fs->cls_map) {
        // 引数のMapをそのまま使う
        int i;
        RefMap *rm = Value_vp(v[1]);
        RefMapReplace *rp = fs->buf_new(cls_replacer, sizeof(RefMapReplace));
        *vret = vp_Value(rp);

        fs->Mem_init(&rp->mem, 1024);

        for (i = 0; i < rm->entry_num; i++) {
            HashValueEntry *et;
            for (et = rm->entry[i]; et != NULL; et = et->next) {
                Str key, val;
                // キー・値がStrまたはBytesでなければエラー
                if (fs->Value_type(et->key) != fs->cls_str || fs->Value_type(et->val) != fs->cls_str) {
                    fs->throw_errorf(fs->mod_lang, "TypeError", "{\"key\":\"value\", ...} required");
                    return TRUE;
                }

                // 検索用のデータ構造
                key = fs->Value_str(et->key);
                val = fs->Value_str(et->val);
                if (key.size > 0) {
                    MapRepl **pp = &rp->map[key.p[0] & 0xFF];
                    MapRepl *p = *pp;

                    // keyが長い順に並べる
                    while (p != NULL) {
                        if (p->key.size <= key.size) {
                            break;
                        }
                        pp = &p->next;
                        p = *pp;
                    }
                    p = fs->Mem_get(&rp->mem, sizeof(MapRepl));
                    p->next = *pp;
                    p->key = key;
                    p->val = val;
                    *pp = p;
                }
            }
        }
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Str or Map required but %n (argument #1)", v1_type);
        return FALSE;
    }

    return TRUE;
}
static int replacer_close(Value *vret, Value *v, RefNode *node)
{
    RefMapReplace *rp = Value_vp(*v);
    fs->Mem_close(&rp->mem);
    return TRUE;
}
static int replacer_replace(Value *vret, Value *v, RefNode *node)
{
    RefMapReplace *rp = Value_vp(*v);
    Str src = fs->Value_str(v[1]);
    StrBuf buf;
    int i = 0;

    fs->StrBuf_init_refstr(&buf, src.size);

    while (i < src.size) {
        int c = src.p[i] & 0xFF;
        MapRepl *p;
        int last = src.size - i;
        int ret;
        for (p = rp->map[c]; p != NULL; p = p->next) {
            // 残りの文字列長とkeyの長さを比較
            if (last >= p->key.size) {
                if (memcmp(src.p + i, p->key.p, p->key.size) == 0) {
                    break;
                }
            }
        }
        if (p != NULL) {
            ret = fs->StrBuf_add(&buf, p->val.p, p->val.size);
            i += p->key.size;
        } else {
            ret = fs->StrBuf_add_c(&buf, c);
            i++;
        }
        if (!ret) {
            return FALSE;
        }
    }
    *vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}
static int replacer_add(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
/*
    RefNode *n;
    RefNode *root = &m->root;

    n = fs->define_identifier(m, root, "convert_table", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, convert_kana, 2, 2, NULL, fs->cls_str, fs->cls_str);
*/
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_replacer = fs->define_identifier(m, m, "Replacer", NODE_CLASS, 0);


    cls = cls_replacer;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, replacer_new, 0, 1, NULL, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, replacer_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "replace", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, replacer_replace, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "add", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, replacer_add, 1, 1, NULL, cls);
    fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_util = m;

    define_func(m);
    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\n";
}

