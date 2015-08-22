#include "fox_vm.h"
#include <string.h>
#include <errno.h>


typedef struct {
    RefHeader rh;

    Mem mem;
    Hash hash;
} RefLoopValidator;


static RefNode *mod_marshal;
static RefNode *cls_loopvalidator;

#define FOX_OBJECT_MAGIC_SIZE 8
static const char *FOX_OBJECT_MAGIC = "\x89\x66\x6f\x78\r\n\0\0";



static int validate_fox_object(Value src)
{
    char magic[8];
    int size = FOX_OBJECT_MAGIC_SIZE;

    if (!stream_read_data(src, NULL, magic, &size, FALSE, FALSE)) {
        return FALSE;
    }
    if (size != 8 || memcmp(magic, FOX_OBJECT_MAGIC, FOX_OBJECT_MAGIC_SIZE) != 0) {
        throw_errorf(fs->mod_io, "FileTypeError", "Not a fox object file");
        return FALSE;
    }
    return TRUE;
}
static void init_marshaldumper(Value *v, Value *src)
{
    RefLoopValidator *lv;
    Ref *r = ref_new(fs->cls_marshaldumper);
    *v = vp_Value(r);
    r->v[INDEX_MARSHALDUMPER_SRC] = *src;

    lv = buf_new(cls_loopvalidator, sizeof(RefLoopValidator));
    r->v[INDEX_MARSHALDUMPER_CYCLREF] = vp_Value(lv);
    Mem_init(&lv->mem, 256);
    Hash_init(&lv->hash, &lv->mem, 16);
}
static int marshal_load(Value *vret, Value *v, RefNode *node)
{
    Value reader;

    if (!value_to_streamio(&reader, v[1], FALSE, 0)) {
        return FALSE;
    }
    if (!validate_fox_object(reader)) {
        Value_dec(reader);
        return FALSE;
    }
    // readerの所有権がMarshalWriterに移る
    init_marshaldumper(fg->stk_top, &reader);
    fg->stk_top++;

    if (!call_member_func(fs->str_read, 0, TRUE)) {
        return FALSE;
    }
    fg->stk_top--;
    *vret = *fg->stk_top;

    return TRUE;
}
static int marshal_save(Value *vret, Value *v, RefNode *node)
{
    Value writer;

    if (!value_to_streamio(&writer, v[1], TRUE, 0)) {
        return FALSE;
    }
    stream_write_data(writer, FOX_OBJECT_MAGIC, FOX_OBJECT_MAGIC_SIZE);

    // writerの所有権がMarshalWriterに移る
    init_marshaldumper(fg->stk_top, &writer);
    fg->stk_top++;
    Value_push("v", v[2]);
    if (!call_member_func(fs->str_write, 1, TRUE)) {
        return FALSE;
    }
    Value_pop();

    return TRUE;
}
static int marshal_bytes(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = bytesio_new_sub(NULL, 256);
    Value writer = vp_Value(mb);
    stream_write_data(writer, FOX_OBJECT_MAGIC, FOX_OBJECT_MAGIC_SIZE);

    // writerの所有権がMarshalWriterに移る
    init_marshaldumper(fg->stk_top, &writer);
    Value_inc(writer);
    fg->stk_top++;
    Value_push("v", v[1]);
    if (!call_member_func(fs->str_write, 1, TRUE)) {
        Value_dec(writer);
        return FALSE;
    }
    Value_pop();

    *vret = cstr_Value(fs->cls_bytes, mb->buf.p, mb->buf.size);
    Value_dec(writer);
    return TRUE;
}

static char *read_str(Str *val, Value r)
{
    uint8_t size;
    int32_t rd_size;
    char *ptr = NULL;

    rd_size = 1;
    if (!stream_read_data(r, NULL, (char*)&size, &rd_size, FALSE, TRUE)) {
        return NULL;
    }
    if (size == 0) {
        throw_errorf(mod_marshal, "MarshalLoadError", "MarshalDumpHeader is broken");
        return NULL;
    }

    rd_size = size;
    ptr = malloc(size);
    if (!stream_read_data(r, NULL, ptr, &rd_size, FALSE, TRUE)) {
        return NULL;
    }
    *val = Str_new(ptr, size);
    return ptr;
}
static int validate_module_name(Str s)
{
    int i;
    int head = TRUE;

    for (i = 0; i < s.size; i++) {
        int ch = s.p[i];
        if (head) {
            if (!islower_fox(ch)) {
                return FALSE;
            }
            head = FALSE;
        } else {
            if (ch == '.') {
                head = TRUE;
            } else if (!islower_fox(ch) && !isdigit_fox(ch) && ch != '_') {
                return FALSE;
            }
        }
    }
    if (head) {
        return FALSE;
    }
    return TRUE;
}
static int validate_class_name(Str s)
{
    int i;
    int valid = FALSE;

    if (s.size == 0) {
        return FALSE;
    }
    if (!isupper_fox(s.p[0])) {
        return FALSE;
    }
    for (i = 1; i < s.size; i++) {
        int ch = s.p[i] & 0xFF;
        if (isalnum(ch)) {
            if (islower(ch)) {
                valid = TRUE;
            }
        } else if (ch == '_') {
        } else {
            return FALSE;
        }
    }

    return valid;
}
static int write_str(RefStr *rs, Value w)
{
    uint8_t size;
    if (rs->size > 255) {
        throw_errorf(mod_marshal, "MarshalSaveError", "Class name or RefNode name too long (> 255)");
        return FALSE;
    }

    size = rs->size;
    if (!stream_write_data(w, (const char*)&size, 1)) {
        return FALSE;
    }
    if (!stream_write_data(w, rs->c, rs->size)) {
        return FALSE;
    }

    return TRUE;
}

static int marshaldump_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value rd = r->v[INDEX_MARSHALDUMPER_SRC];
    char *ptr;
    Str name_s;
    RefStr *name_r;
    RefNode *cls;
    RefNode *mod;

    ptr = read_str(&name_s, rd);
    if (ptr == NULL) {
        goto ERROR_END;
    }
    if (!validate_module_name(name_s)) {
        goto ERROR_END;
    }
    mod = get_module_by_name(name_s.p, name_s.size, FALSE, TRUE);
    free(ptr);
    if (mod == NULL) {
        goto ERROR_END;
    }

    ptr = read_str(&name_s, rd);
    if (ptr == NULL) {
        goto ERROR_END;
    }
    if (!validate_class_name(name_s)) {
        goto ERROR_END;
    }
    name_r = intern(name_s.p, name_s.size);
    cls = Hash_get_p(&mod->u.m.h, name_r);
    if (cls == NULL) {
        throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, mod, name_r);
        free(ptr);
        goto ERROR_END;
    }
    free(ptr);

    if (cls == fs->cls_module || cls == fs->cls_class) {
        RefNode *cls2;
        RefNode *mod2;
        ptr = read_str(&name_s, rd);
        if (ptr == NULL) {
            goto ERROR_END;
        }
        if (!validate_module_name(name_s)) {
            goto ERROR_END;
        }
        mod2 = get_module_by_name(name_s.p, name_s.size, FALSE, TRUE);
        free(ptr);
        if (mod2 == NULL) {
            goto ERROR_END;
        }
        if (cls == fs->cls_module) {
            *vret = vp_Value(mod2);
        } else {
            ptr = read_str(&name_s, rd);
            if (ptr == NULL) {
                goto ERROR_END;
            }
            if (!validate_class_name(name_s)) {
                goto ERROR_END;
            }
            name_r = intern(name_s.p, name_s.size);
            cls2 = Hash_get(&mod2->u.m.h, name_s.p, name_s.size);
            if (cls2 == NULL) {
                throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, mod, name_r);
                free(ptr);
                goto ERROR_END;
            }
            free(ptr);
            *vret = vp_Value(cls2);
        }
    } else {
        Value_push("rv", cls, *v);
        if (!call_member_func(fs->str_marshal_read, 1, TRUE)) {
            return FALSE;
        }
        fg->stk_top--;
        *vret = *fg->stk_top;
    }

    return TRUE;

ERROR_END:
    if (fg->error == VALUE_NULL) {
        // Errorがセットされていないが、エラーで飛んできたので、何かセットする
        throw_errorf(mod_marshal, "MarshalLoadError", "MarshalDumpHeader is broken");
    }
    return FALSE;
}
static int marshaldump_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value w = r->v[INDEX_MARSHALDUMPER_SRC];
    Value v1 = v[1];

    if (Value_isref(v1)) {
        RefLoopValidator *lv = Value_vp(r->v[INDEX_MARSHALDUMPER_CYCLREF]);
        Ref *r1 = Value_ref(v1);
        HashEntry *he = Hash_get_add_entry(&lv->hash, &lv->mem, (RefStr*)r1);

        if (he->p != NULL) {
            throw_error_select(THROW_LOOP_REFERENCE);
            return FALSE;
        } else {
            he->p = (void*)1;
        }
    }

    {
        const RefNode *cls = Value_type(v1);
        RefStr *mod_name = cls->defined_module->name;

        if (mod_name == fs->str_toplevel) {
            throw_errorf(fs->mod_lang, "ValueError", "Cannot save object which belongs no modules");
            return FALSE;
        }

        if (!write_str(mod_name, w)) {
            return FALSE;
        }
        if (!write_str(cls->name, w)) {
            return FALSE;
        }

        if (cls == fs->cls_module) {
            RefNode *mod2 = Value_vp(v1);
            if (!write_str(mod2->name, w)) {
                return FALSE;
            }
        } else if (cls == fs->cls_class) {
            RefNode *cls2 = Value_vp(v1);
            if (!write_str(cls2->defined_module->name, w)) {
                return FALSE;
            }
            if (!write_str(cls2->name, w)) {
                return FALSE;
            }
        } else {
            Value_push("vv", v1, *v);
            if (!call_member_func(fs->str_marshal_write, 1, TRUE)) {
                return FALSE;
            }
            Value_pop();
        }
    }

    return TRUE;
}
static int marshaldump_stream(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = Value_cp(r->v[INDEX_MARSHALDUMPER_SRC]);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * v:Ref値を渡す
 */
static int value_has_loop_sub(Value v, Hash *hash, Mem *mem)
{
    HashEntry *he;
    Ref *r = Value_ref(v);
    const RefNode *type = r->rh.type;

    if (type == NULL) {
        return FALSE;
    }
    he = Hash_get_add_entry(hash, mem, (RefStr*)r);

    if (he->p != NULL) {
        return TRUE;
    } else {
        he->p = (void*)1;
    }

    if (type == fs->cls_list) {
        int i;
        RefArray *ra = Value_vp(v);
        for (i = 0; i < ra->size; i++) {
            Value v1 = ra->p[i];
            if (Value_isref(v1) && value_has_loop_sub(v1, hash, mem)) {
                return TRUE;
            }
        }
    } else if (type == fs->cls_map) {
        // SetはMapとして構築される
        int i;
        RefMap *rm = Value_vp(v);

        for (i = 0; i < rm->entry_num; i++) {
            HashValueEntry *ve = rm->entry[i];
            for (; ve != NULL; ve = ve->next) {
                if (Value_isref(ve->key) && value_has_loop_sub(ve->key, hash, mem)) {
                    return TRUE;
                }
                if (Value_isref(ve->val) && value_has_loop_sub(ve->val, hash, mem)) {
                    return TRUE;
                }
            }
        }
    } else {
        int i;
        for (i = 0; i < type->u.c.n_memb; i++) {
            Value v1 = r->v[i];
            if (Value_isref(v1) && value_has_loop_sub(v1, hash, mem)) {
                return TRUE;
            }
        }
    }
    return FALSE;
}
static int marshal_validate_loop_ref(Value *vret, Value *v, RefNode *node)
{
    int ret = FALSE;

    if (Value_isref(v[1])) {
        Mem mem;
        Hash hash;

        Mem_init(&mem, 1024);
        Hash_init(&hash, &mem, 128);
        ret = value_has_loop_sub(v[1], &hash, &mem);

        Mem_close(&mem);
    }
    if (ret) {
        throw_error_select(THROW_LOOP_REFERENCE);
        return FALSE;
    }

    return TRUE;
}
static int loopref_new(Value *vret, Value *v, RefNode *node)
{
    RefLoopValidator *lv = buf_new(cls_loopvalidator, sizeof(RefLoopValidator));
    *vret = vp_Value(lv);

    Mem_init(&lv->mem, 1024);
    Hash_init(&lv->hash, &lv->mem, 128);

    return TRUE;
}
static int loopref_dispose(Value *vret, Value *v, RefNode *node)
{
    RefLoopValidator *lv = Value_vp(*v);

    if (lv->mem.p != NULL) {
        Mem_close(&lv->mem);
        lv->mem.p = NULL;
    }
    return TRUE;
}
static int loopref_push(Value *vret, Value *v, RefNode *node)
{
    RefLoopValidator *lv = Value_vp(*v);
    Value v1 = v[1];

    if (Value_isref(v1)) {
        Ref *r = Value_ref(v1);
        HashEntry *he = Hash_get_add_entry(&lv->hash, &lv->mem, (RefStr*)r);

        if (he->p != NULL) {
            throw_error_select(THROW_LOOP_REFERENCE);
            return FALSE;
        } else {
            he->p = (void*)1;
        }
    }
    return TRUE;
}
static int loopref_pop(Value *vret, Value *v, RefNode *node)
{
    RefLoopValidator *lv = Value_vp(*v);
    Value v1 = v[1];

    if (Value_isref(v1)) {
        Ref *r = Value_ref(v1);
        HashEntry *he = Hash_get_add_entry(&lv->hash, &lv->mem, (RefStr*)r);

        if (he->p == NULL) {
            throw_errorf(fs->mod_lang, "ValueError", "Wrong value given");
            return FALSE;
        } else {
            he->p = (void*)0;
        }
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static void define_marshal_func(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "marshal_load", NODE_FUNC_N, 0);
    define_native_func_a(n, marshal_load, 1, 1, NULL, NULL);

    n = define_identifier(m, m, "marshal_save", NODE_FUNC_N, 0);
    define_native_func_a(n, marshal_save, 2, 2, NULL, NULL, NULL);

    n = define_identifier(m, m, "marshal_bytes", NODE_FUNC_N, 0);
    define_native_func_a(n, marshal_bytes, 1, 1, NULL, NULL);


    // 循環参照しているか調べる
    n = define_identifier(m, m, "validate_loop_ref", NODE_FUNC_N, 0);
    define_native_func_a(n, marshal_validate_loop_ref, 1, 1, NULL, NULL);
}

void define_marshal_class(RefNode *m)
{
    RefNode *cls, *cls2;
    RefNode *n;

    cls_loopvalidator = define_identifier(m, m, "LoopValidator", NODE_CLASS, 0);
    cls = cls_loopvalidator;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, loopref_new, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, loopref_dispose, 0, 0, NULL);
    n = define_identifier(m, cls, "push", NODE_FUNC_N, 0);
    define_native_func_a(n, loopref_push, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "pop", NODE_FUNC_N, 0);
    define_native_func_a(n, loopref_pop, 1, 1, NULL, NULL);
    extends_method(cls, fs->cls_obj);


    cls = fs->cls_marshaldumper;
    n = define_identifier_p(m, cls, fs->str_read, NODE_FUNC_N, 0);
    define_native_func_a(n, marshaldump_read, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_write, NODE_FUNC_N, 0);
    define_native_func_a(n, marshaldump_write, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "stream", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, marshaldump_stream, 1, 1, NULL, NULL);
    cls->u.c.n_memb = INDEX_MARSHALDUMPER_NUM;
    extends_method(cls, fs->cls_obj);


    cls = define_identifier(m, m, "MarshalError", NODE_CLASS, NODEOPT_ABSTRACT);
    define_error_class(cls, fs->cls_error, m);
    cls2 = cls;

    cls = define_identifier(m, m, "MarshalLoadError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);
}

RefNode *init_marshal_module_stubs()
{
    RefNode *m = Module_new_sys("marshal");
    fs->cls_marshaldumper = define_identifier(m, m, "MarshalDumper", NODE_CLASS, 0);
    return m;
}
void init_marshal_module_1(RefNode *m)
{
    define_marshal_class(m);
    define_marshal_func(m);
    mod_marshal = m;
}
