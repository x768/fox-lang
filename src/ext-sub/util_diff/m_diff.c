#include "fox.h"


enum {
    INDEX_COMPFILE_FILE,
    INDEX_COMPFILE_HASH,
    INDEX_COMPFILE_SIZE,
    INDEX_COMPFILE_NUM,
};

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_diff;
static RefNode *cls_cmpfile;
static RefNode *cls_cmpfileset;


static int load_file(Ref *r, const char *path)
{
    char buf[4096];
    int read_size = -1;
    int32_t hash = 0;
    int64_t size = 0;
    FileHandle fd = open_fox(path, O_RDONLY);
    if (fd == -1) {
        return FALSE;
    }

    while ((read_size = read_fox(fd, buf, sizeof(buf))) > 0) {
        int i;
        for (i = 0; i < read_size; i++) {
            hash = hash * 31 + buf[i];
        }
        size += read_size;
    }
    r->v[INDEX_COMPFILE_HASH] = int32_Value(hash & 0x7fffFFFF);
    r->v[INDEX_COMPFILE_SIZE] = uint62_Value(size);

    return TRUE;
}

static int cmpfile_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = fs->ref_new(cls_cmpfile);
    RefNode *v_type = fs->Value_type(v[1]);

    *vret = vp_Value(r);
    
    if (v_type == fs->cls_str) {
        fs->Value_push("rv", fs->cls_file, v[1]);
        if (!fs->call_member_func(fs->str_new, 1, TRUE)) {
            return FALSE;
        }
        r->v[INDEX_COMPFILE_FILE] = fg->stk_top[-1];
        fg->stk_top--;
    } else if (fs->is_subclass(v_type, fs->cls_file)) {
        r->v[INDEX_COMPFILE_FILE] = fs->Value_cp(v[1]);
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_file, v_type);
        return FALSE;
    }
    if (!load_file(r, Value_cstr(r->v[INDEX_COMPFILE_FILE]))) {
        return FALSE;
    }

    return TRUE;
}
static int cmpfile_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = fs->printf_Value("CmpFile(%s)", Value_cstr(r->v[INDEX_COMPFILE_FILE]));
    return TRUE;
}
static int cmpfile_hash(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    const char *fname = Value_cstr(r->v[INDEX_COMPFILE_FILE]);
    int i;
    int32_t hash = 0;

    for (i = 0; fname[i] != '\0'; i++) {
        hash = hash * 31 + fname[i];
    }
    hash += r->v[INDEX_COMPFILE_SIZE];
    hash += r->v[INDEX_COMPFILE_HASH];

    *vret = int32_Value(hash & 0x7fffFFFF);
 
    return TRUE;
}
static int cmpfile_file(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = fs->Value_cp(r->v[INDEX_COMPFILE_FILE]);
    return TRUE;
}
static int cmpfile_size(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int64_t size = fs->Value_int64(r->v[INDEX_COMPFILE_SIZE], NULL);
    *vret = fs->int64_Value(size);
    return TRUE;
}
static int cmpfile_filehash(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = r->v[INDEX_COMPFILE_HASH];
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int cmpfileset_new(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_cmpfile = fs->define_identifier(m, m, "CmpFile", NODE_CLASS, 0);
    cls_cmpfileset = fs->define_identifier(m, m, "CmpFileSet", NODE_CLASS, 0);

    cls = cls_cmpfile;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, cmpfile_new, 1, 1, NULL, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cmpfile_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cmpfile_hash, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "file", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cmpfile_file, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cmpfile_size, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "filehash", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cmpfile_filehash, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_COMPFILE_NUM;
    fs->extends_method(cls, fs->cls_obj);

    cls = cls_cmpfileset;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, cmpfileset_new, 1, 1, NULL, fs->cls_bool);
    
    fs->extends_method(cls, fs->cls_obj);
}

static void define_func(RefNode *m)
{
//    RefNode *n;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_diff = m;

    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}
