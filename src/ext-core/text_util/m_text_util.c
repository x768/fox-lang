#define DEFINE_GLOBALS
#include "m_text_util.h"
#include "m_xml.h"
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


static RefNode *cls_replacer;
static RefNode *cls_hilighter;
static RefNode *cls_hilight_iter;
static RefNode *cls_elem;
static RefNode *cls_text;

static RefStr *str_span;
static RefStr *str_class;
static RefStr *str__;


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
    size = get_file_size(fh);
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
    const RefNode *v1_type = fs->Value_type(v[1]);

    if (v1_type == fs->cls_str) {
        // ファイルから読む
        RefStr *path = Value_vp(v[1]);
        char *path_p = fs->resource_to_path(Str_new(path->c, path->size), ".txt");
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
                RefStr *key, *val;
                // キー・値がStrまたはBytesでなければエラー
                if (fs->Value_type(et->key) != fs->cls_str || fs->Value_type(et->val) != fs->cls_str) {
                    fs->throw_errorf(fs->mod_lang, "TypeError", "{\"key\":\"value\", ...} required");
                    return TRUE;
                }

                // 検索用のデータ構造
                key = Value_vp(et->key);
                val = Value_vp(et->val);
                if (key->size > 0) {
                    MapRepl **pp = &rp->map[key->c[0] & 0xFF];
                    MapRepl *p = *pp;

                    // keyが長い順に並べる
                    while (p != NULL) {
                        if (p->key.size <= key->size) {
                            break;
                        }
                        pp = &p->next;
                        p = *pp;
                    }
                    p = fs->Mem_get(&rp->mem, sizeof(MapRepl));
                    p->next = *pp;
                    p->key = Str_new(key->c, key->size);
                    p->val = Str_new(val->c, val->size);
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
    RefStr *src = Value_vp(v[1]);
    StrBuf buf;
    int i = 0;

    fs->StrBuf_init_refstr(&buf, src->size);

    while (i < src->size) {
        int c = src->c[i] & 0xFF;
        MapRepl *p;
        int last = src->size - i;
        int ret;
        for (p = rp->map[c]; p != NULL; p = p->next) {
            // 残りの文字列長とkeyの長さを比較
            if (last >= p->key.size) {
                if (memcmp(src->c + i, p->key.p, p->key.size) == 0) {
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

///////////////////////////////////////////////////////////////////////////////////////////////////

static int str_isalnumu(const char *src)
{
    for (; *src != '\0'; src++) {
        if (!isalnumu_fox(*src)) {
            return FALSE;
        }
    }
    return TRUE;
}

static int hilighter_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *v1_type = fs->Value_type(v[1]);
    RefStr *rs_type = NULL;

    if (v1_type == fs->cls_mimetype) {
        rs_type = Value_vp(v[1]);
    } else if (v1_type == fs->cls_str) {
        rs_type = Value_vp(v[1]);
        if (str_isalnumu(rs_type->c)) {
            char cbuf[16];
            sprintf(cbuf, ".%.14s", rs_type->c);
            // 拡張子->MimeType
            rs_type = fs->mimetype_from_suffix(cbuf, -1);
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_mimetype, fs->cls_str, v1_type, 1);
        return FALSE;
    }

    {
        Hilight *h = fs->buf_new(cls_hilighter, sizeof(Hilight));
        fs->Mem_init(&h->mem, 1024);
        if (!load_hilight(h, rs_type)) {
            return FALSE;
        }
        h->mimetype = fs->cstr_Value(fs->cls_mimetype, rs_type->c, rs_type->size);
        *vret = vp_Value(h);
    }
    return TRUE;
}
static int hilighter_get_mime(Value *vret, Value *v, RefNode *node)
{
    Hilight *h = Value_vp(v[0]);
    *vret = fs->Value_cp(h->mimetype);
    return TRUE;
}
static int hilighter_parse(Value *vret, Value *v, RefNode *node)
{
    Hilight *h = Value_vp(v[0]);
    HilightIter *it = fs->buf_new(cls_hilight_iter, sizeof(HilightIter));

    RefStr *src = Value_vp(v[1]);

    it->h_val = fs->Value_cp(v[0]);
    it->src_val = fs->Value_cp(v[1]);
    it->h = h;
    it->src_end = src->c + src->size;
    it->ptr = src->c;
    it->end = src->c;
    it->word = NULL;
    it->next_word = NULL;
    it->state = fs->Hash_get(&h->state, "_", 1);
    it->next_state = it->state;
    it->state_next1 = it->state;
    it->state_next2 = it->state;

    *vret = vp_Value(it);
    return TRUE;
}
static int hilighter_close(Value *vret, Value *v, RefNode *node)
{
    Hilight *h = Value_vp(v[0]);
    fs->Mem_close(&h->mem);
    return TRUE;
}

static Ref *xmlspan_new(RefStr *klass, const char *src, int src_len)
{
    RefArray *ra;
    RefMap *rm;
    HashValueEntry *em;
    Ref *r = fs->ref_new(cls_elem);

    ra = fs->refarray_new(0);
    *fs->refarray_push(ra) = fs->cstr_Value(cls_text, src, src_len);

    rm = fs->refmap_new(1);
    em = fs->refmap_add(rm, vp_Value(str_class), TRUE, FALSE);
    em->val = vp_Value(klass);

    r->v[INDEX_ELEM_NAME] = vp_Value(str_span);
    r->v[INDEX_ELEM_CHILDREN] = vp_Value(ra);
    r->v[INDEX_ELEM_ATTR] = vp_Value(rm);

    return r;
}

static int hilight_iter_next(Value *vret, Value *v, RefNode *node)
{
    HilightIter *it = Value_vp(v[0]);
    const char *begin = NULL;
    const char *end = NULL;

    // 同じ色をまとめる
    while (hilight_code_next(it)) {
        if (begin == NULL) {
            begin = it->ptr;
        }
        end = it->end;
        if (it->state != it->state_next1) {
            break;
        }
    }

    if (begin == NULL) {
        fs->throw_stopiter();
        return FALSE;
    }

    if (it->state->name != str__) {
        Ref *r = xmlspan_new(it->state->name, begin, (int)(end - begin));
        *vret = vp_Value(r);
    } else {
        *vret = fs->cstr_Value(cls_text, begin, (int)(end - begin));
    }
    return TRUE;
}
static int hilight_iter_close(Value *vret, Value *v, RefNode *node)
{
    HilightIter *it = Value_vp(v[0]);
    fs->unref(it->src_val);
    fs->unref(it->h_val);
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_replacer = fs->define_identifier(m, m, "Replacer", NODE_CLASS, 0);
    cls_hilighter = fs->define_identifier(m, m, "Hilighter", NODE_CLASS, 0);
    cls_hilight_iter = fs->define_identifier(m, m, "HilightIter", NODE_CLASS, 0);


    cls = cls_replacer;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, replacer_new, 1, 1, NULL, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, replacer_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "replace", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, replacer_replace, 1, 1, NULL, fs->cls_str);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_hilighter;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, hilighter_new, 1, 1, NULL, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, hilighter_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "mimetype", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, hilighter_get_mime, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "parse", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, hilighter_parse, 1, 1, NULL, fs->cls_str);
    fs->extends_method(cls, fs->cls_obj);

    cls = cls_hilight_iter;
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, hilight_iter_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, hilight_iter_next, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_iterator);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_util = m;

    {
        RefNode *mod_xml = fs->get_module_by_name("marshal.xml", -1, TRUE, FALSE);
        cls_elem = fs->Hash_get(&mod_xml->u.m.h, "XMLElem", -1);
        cls_text = fs->Hash_get(&mod_xml->u.m.h, "XMLText", -1);
    }
    str_span = fs->intern("span", 4);
    str_class = fs->intern("class", 5);
    str__ = fs->intern("_", 1);


    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}

