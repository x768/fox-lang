#define DEFINE_GLOBALS
#include "fox.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_reflect;

static int ref_get_member(Value *vret, Value *v, RefNode *node)
{
    RefStr *name = Value_vp(v[2]);
    RefStr *name_r = fs->get_intern(name->c, name->size);

    if (name_r == NULL) {
        name_r = name;
    }
    if (name_r->size > 0 && name_r->c[0] == '_') {
        fs->throw_errorf(fs->mod_lang, "DefineError", "%r is private", name_r);
        return FALSE;
    }
    *fg->stk_top++ = fs->Value_cp(v[1]);

    if (!fs->call_property(name_r)) {
        return FALSE;
    }
    *vret = fg->stk_top[-1];
    fg->stk_top--;

    return TRUE;
}
static int ref_get_member_list(Value *vret, Value *v, RefNode *node)
{
    int name_only = FUNC_INT(node);
    RefNode *type = fs->Value_type(v[1]);
    RefMap *rm = fs->refmap_new(0);
    RefNode *nd_src;
    int i;

    if (name_only) {
        rm->rh.type = fs->cls_set;
    }
    *vret = vp_Value(rm);

    if (type == fs->cls_module || type == fs->cls_class) {
        nd_src = Value_vp(v[1]);
    } else {
        nd_src = type;
    }
    for (i = 0; i < nd_src->u.c.h.entry_num; i++) {
        HashEntry *entry;
        for (entry = nd_src->u.c.h.entry[i]; entry != NULL; entry = entry->next) {
            RefStr *name = entry->key;
            if (name->c[0] != '_') {
                RefNode *nd = entry->p;
                if (type == fs->cls_module) {
                } else if (type == fs->cls_class) {
                    if ((nd->type & NODEMASK_MEMBER_S) == 0) {
                        continue;
                    }
                } else {
                    if ((nd->type & NODEMASK_MEMBER) == 0) {
                        continue;
                    }
                }
                {
                    HashValueEntry *e = fs->refmap_add(rm, vp_Value(name), FALSE, FALSE);
                    if (!name_only) {
                        fs->Value_push("v", v[1]);
                        if (!fs->call_property(name)) {
                            return FALSE;
                        }
                        e->val = fg->stk_top[-1];
                        fg->stk_top--;
                    }
                }
            }
        }
    }

    return TRUE;
}
static int get_declaring_class(Value *vret, Value *v, RefNode *node)
{
    Ref *fn_obj = Value_ref(v[1]);
    RefNode *fn;
    RefNode *cls;

    if (fn_obj->rh.n_memb > 0) {
        fn = Value_vp(fn_obj->v[INDEX_FUNC_FN]);
    } else {
        fn = Value_vp(v[1]);
    }

    cls = fn->u.f.klass;
    if (cls == NULL || cls->rh.type != fs->cls_class) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Not a class method");
        return FALSE;
    }
    if (cls != NULL){
        *vret = vp_Value(cls);
    }
    return TRUE;
}
static int get_declaring_module(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = fs->Value_type(v[1]);
    RefNode *module;

    if (type == fs->cls_class) {
        RefNode *cls = Value_vp(v[1]);
        module = cls->defined_module;
    } else if (type == fs->cls_fn) {
        Ref *fn_obj = Value_ref(v[1]);
        if (fn_obj->rh.n_memb > 0) {
            RefNode *fn = Value_vp(fn_obj->v[INDEX_FUNC_FN]);
            module = fn->defined_module;
        } else {
            RefNode *fn = Value_vp(v[1]);
            module = fn->defined_module;
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_class, fs->cls_fn, type, 1);
        return FALSE;
    }
    if (module != NULL) {
        *vret = vp_Value(module);
    }
    return TRUE;
}
static int ref_get_this(Value *vret, Value *v, RefNode *node)
{
    Ref *fn_obj = Value_ref(v[1]);

    if (fn_obj->rh.n_memb == 0) {
        *vret = VALUE_NULL;
    } else {
        *vret = fs->Value_cp(fn_obj->v[INDEX_FUNC_THIS]);
    }
    return TRUE;
}
static int ref_get_name(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefNode *type = fs->Value_type(v1);

    if (type == fs->cls_module || type == fs->cls_class || type == fs->cls_fn) {
        RefNode *n = Value_vp(v1);
        *vret = fs->Value_cp(vp_Value(n->name));
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Module, Class or Function required but %n (argument #1)", type);
    }
    return TRUE;
}
static int get_module_path(Value *vret, Value *v, RefNode *node)
{
    RefNode *module = Value_vp(v[1]);

    if (module->u.m.src_path != NULL) {
        *vret = fs->cstr_Value(NULL, module->u.m.src_path, -1);
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static char *convert_module_name(RefStr *rs)
{
    char *p = fs->str_dup_p(rs->c, rs->size, NULL);
    int i;
    int dot = TRUE;

    if (rs->size == 0) {
        return p;
    }

    for (i = 0; p[i] != '\0'; i++) {
        if (p[i] == '.') {
            if (dot) {
                goto ERROR_END;
            }
            p[i] = SEP_C;
            dot = TRUE;
        } else {
            if (!islower_fox(p[i]) && !isdigit_fox(p[i]) && p[i] != '_') {
                goto ERROR_END;
            }
            p[i] = tolower_fox(p[i]);
            dot = FALSE;
        }
    }
    if (dot) {
        goto ERROR_END;
    }
    return p;

ERROR_END:
    free(p);
    return NULL;
}
static int is_valid_module_name(const char *p)
{
    int i;

    for (i = 0; p[i] != '\0'; i++) {
        if (!islower_fox(p[i]) && !isdigit_fox(p[i]) && p[i] != '_') {
            return FALSE;
        }
    }
    return TRUE;
}
static int file_name_to_module_name(char *p)
{
    int i;

    for (i = 0; p[i] != '\0'; i++) {
        if (p[i] == '.') {
            if (i > 0) {
                break;
            } else {
                return FALSE;
            }
        }
        if (!islower_fox(p[i]) && p[i] != '_') {
            return FALSE;
        }
    }
    if (strcmp(&p[i], ".fox") == 0 || strcmp(&p[i], ".so") == 0) {
        p[i] = '\0';
        return TRUE;
    }
    return FALSE;
}
static char *concat_module_path(const char *base, const char *name, int sep)
{
    if (base[0] == '\0') {
        int length = strlen(name);
        char *p = malloc(length + 1);
        strcpy(p, name);
        return p;
    } else if (name[0] == '\0') {
        int length = strlen(base);
        char *p = malloc(length + 1);
        strcpy(p, base);
        return p;
    } else {
        int length = strlen(base) + strlen(name);
        char *p = malloc(length + 2);
        sprintf(p, "%s%c%s", base, sep, name);
        return p;
    }
}
static int get_module_list(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    char *name = convert_module_name(rs);
    RefArray *ra = fs->refarray_new(0);
    int find_dirs = FUNC_INT(node);

    PtrList *imp_path;
    
    *vret = vp_Value(ra);

    if (name == NULL) {
        fs->throw_errorf(fs->mod_lang, "ImportError", "Cannot find directories for '%r'", Value_vp(v[1]));
        return FALSE;
    }

    for (imp_path = fs->import_path; imp_path != NULL; imp_path = imp_path->next) {
        char *path = concat_module_path(imp_path->u.c, name, SEP_C);
        DIR *d = opendir_fox(path);
        if (d != NULL) {
            struct dirent *de;
            while ((de = readdir_fox(d)) != NULL) {
                if (find_dirs) {
                    if ((de->d_type & DT_DIR) != 0 && is_valid_module_name(de->d_name)) {
                        Value *vp = fs->refarray_push(ra);
                        if (rs->size > 0) {
                            int length = rs->size + strlen(de->d_name) + 1;
                            RefStr *rs2 = fs->refstr_new_n(fs->cls_str, length);
                            sprintf(rs2->c, "%s.%s", rs->c, de->d_name);
                            *vp = vp_Value(rs2);
                        } else {
                            *vp = fs->cstr_Value(fs->cls_str, de->d_name, -1);
                        }
                    }
                } else {
                    if ((de->d_type & DT_REG) != 0 && file_name_to_module_name(de->d_name)) {
                        Value *vp = fs->refarray_push(ra);
                        RefNode *mod = NULL;
                        if (rs->size > 0) {
                            char *mod_name = concat_module_path(rs->c, de->d_name, '.');
                            mod = fs->get_module_by_name(mod_name, -1, FALSE, TRUE);
                            free(mod_name);
                        } else {
                            mod = fs->get_module_by_name(de->d_name, -1, FALSE, TRUE);
                        }
                        if (mod != NULL) {
                            *vp = vp_Value(mod);
                        } else {
                            *vp = VALUE_NULL;
                            closedir_fox(d);
                            free(path);
                            return FALSE;
                        }
                    }
                }
            }
            closedir_fox(d);
        }
        free(path);
    }

    free(name);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    // get_member(v, 'hoge') == v.hoge
    n = fs->define_identifier(m, m, "get_member", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ref_get_member, 2, 2, NULL, NULL, fs->cls_str);

    n = fs->define_identifier(m, m, "get_member_names", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ref_get_member_list, 1, 1, (void*)TRUE, NULL);

    n = fs->define_identifier(m, m, "get_member_map", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ref_get_member_list, 1, 1, (void*)FALSE, NULL);

    n = fs->define_identifier(m, m, "get_declaring_class", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_declaring_class, 1, 1, NULL, fs->cls_fn);

    n = fs->define_identifier(m, m, "get_declaring_module", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_declaring_module, 1, 1, NULL, NULL);

    n = fs->define_identifier(m, m, "get_this", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ref_get_this, 1, 1, NULL, fs->cls_fn);

    n = fs->define_identifier(m, m, "get_name", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ref_get_name, 1, 1, NULL, NULL);

    n = fs->define_identifier(m, m, "get_module_path", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_module_path, 1, 1, NULL, fs->cls_module);

    n = fs->define_identifier(m, m, "get_module_dirs", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_module_list, 1, 1, (void*)TRUE, fs->cls_str);

    n = fs->define_identifier(m, m, "get_module_list", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_module_list, 1, 1, (void*)FALSE, fs->cls_str);
}
static void define_class(RefNode *m)
{
//  RefNode *cls;
//  RefNode *n;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_reflect = m;

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
