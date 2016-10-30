#include "fox_vm.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


enum {
    INDEX_ERROR_STACKTRACE,
    INDEX_ERROR_MESSAGE,
    INDEX_ERROR_NUM,
};

// class WeakRef
enum {
    INDEX_WEAKREF_REF,
    INDEX_WEAKREF_ALIVE,
    INDEX_WEAKREF_NUM,
};

typedef struct // 例外のスタックトレース(PtrArrayを使う)
{
    RefNode *module;
    RefNode *func;
    int line;
} StackTrace;


static RefNode *cls_weakref;

RefNode *define_identifier(RefNode *module, RefNode *klass, const char *name, int type, int opt)
{
    RefNode *n = Node_define(klass, module, intern(name, -1), type, NULL);
    if (type >= NODE_FUNC) {
        n->u.f.klass = klass;
    } else {
        n->u.f.klass = NULL;
    }
    n->opt = opt;
    if ((opt & NODEOPT_INTEGRAL) != 0) {
        if (fv->integral_num >= fv->integral_max) {
            fv->integral_max *= 2;
            fv->integral = realloc(fv->integral, sizeof(RefNode*) * fv->integral_max);
        }
        n->u.c.n_integral = fv->integral_num;
        fv->integral[fv->integral_num++] = n;
    }
    return n;
}
void redefine_identifier(RefNode *module, RefNode *klass, const char *name, int type, int opt, RefNode *node)
{
    RefNode *n = Node_define(klass, module, intern(name, -1), type, node);
    if (type >= NODE_FUNC) {
        n->u.f.klass = klass;
    } else {
        n->u.f.klass = NULL;
    }
    n->opt = opt;
}
RefNode *define_identifier_p(RefNode *module, RefNode *klass, RefStr *pn, int type, int opt)
{
    RefNode *n = Node_define(klass, module, pn, type, NULL);
    if (type >= NODE_FUNC) {
        n->u.f.klass = klass;
    } else {
        n->u.f.klass = NULL;
    }
    n->opt = opt;

    return n;
}

static int32_t Value_get_hash(Value v)
{
    int32_t i = (v & 0xFFFFffff) ^ (v >> 32);
    return int32_hash(i);
}

///////////////////////////////////////////////////////////////////////////////////////////////

int native_return_null(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}
int native_return_bool(Value *vret, Value *v, RefNode *node)
{
    *vret = bool_Value(FUNC_INT(node));
    return TRUE;
}
int native_return_this(Value *vret, Value *v, RefNode *node)
{
    *vret = Value_cp(*v);
    return TRUE;
}

void define_native_func_a(RefNode *node, NativeFunc func, int argc_min, int argc_max, void *vp, ...)
{
    int ck_argc = (argc_max >= 0 ? argc_max : argc_min);
    RefNode **arg_type = Mem_get(&fg->st_mem, ck_argc * sizeof(RefNode*));

    node->u.f.arg_min = argc_min;
    node->u.f.arg_max = argc_max;
    node->u.f.max_stack = 0;

    if (ck_argc > 0) {
        va_list va;
        int i;

        va_start(va, vp);
        for (i = 0; i < ck_argc; i++) {
            arg_type[i] = va_arg(va, RefNode*);
        }
        va_end(va);
        node->u.f.arg_type = arg_type;
    } else {
        node->u.f.arg_type = NULL;
    }

    node->u.f.u.fn = func;
    node->u.f.vp = vp;
}
int native_get_member(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int idx = FUNC_INT(node);
    *vret = Value_cp(r->v[idx]);
    return TRUE;
}
int native_set_member(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int idx = FUNC_INT(node);
    r->v[idx] = Value_cp(v[1]);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int lang_typeof(Value *vret, Value *v, RefNode *node)
{
    if (v[1] == 3ULL) {
        throw_errorf(fs->mod_lang, "ValueError", "???");
        return FALSE;
    }
    *vret = vp_Value(Value_type(v[1]));
    return TRUE;
}
static int lang_ref_count(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    int count = 0;

    if (Value_isref(v1)) {
        count = Value_ref_header(v1)->nref;
    }
    *vret = int32_Value(count);

    return TRUE;
}
static int lang_heap_count(Value *vret, Value *v, RefNode *node)
{
    *vret = int32_Value(fv->heap_count);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int object_hash(Value *vret, Value *v, RefNode *node)
{
    int32_t hash = Value_get_hash(*v);
    *vret = int32_Value(hash & INT32_MAX);
    return TRUE;
}
static int object_tostr(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = Value_type(*v);
    *vret = printf_Value("%n(#%x)", type, Value_get_hash(*v));
    return TRUE;
}
int object_eq(Value *vret, Value *v, RefNode *node)
{
    *vret = bool_Value(*v == v[1]);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int null_tostr(Value *vret, Value *v, RefNode *node)
{
    *vret = vp_Value(fs->str_0);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int bool_new(Value *vret, Value *v, RefNode *node)
{
    if (fg->stk_top > v + 1 && Value_bool(v[1])) {
        *vret = VALUE_TRUE;
    } else {
        *vret = VALUE_FALSE;
    }
    return TRUE;
}
static int bool_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    int size = 1;
    char buf;

    if (!stream_read_data(r, NULL, &buf, &size, FALSE, TRUE)) {
        return FALSE;
    }
    *vret = bool_Value(buf);
    return TRUE;
}
static int bool_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    char b = Value_bool(*v) ? 1 : 0;

    if (!stream_write_data(w, &b, 1)) {
        return FALSE;
    }
    return TRUE;
}
static int bool_tostr(Value *vret, Value *v, RefNode *node)
{
    if (Value_bool(*v)) {
        *vret = vp_Value(intern("true", 4));
    } else {
        *vret = vp_Value(intern("false", 5));
    }

    return TRUE;
}
static int bool_empty(Value *vret, Value *v, RefNode *node)
{
    int b = Value_bool(*v);
    *vret = bool_Value(!b);
    return TRUE;
}
static int bool_and(Value *vret, Value *v, RefNode *node)
{
    int b = Value_bool(v[0]) && Value_bool(v[1]);
    *vret = bool_Value(b);
    return TRUE;
}
static int bool_or(Value *vret, Value *v, RefNode *node)
{
    int b = Value_bool(v[0]) || Value_bool(v[1]);
    *vret = bool_Value(b);
    return TRUE;
}
static int bool_xor(Value *vret, Value *v, RefNode *node)
{
    int b = Value_bool(v[0]) != Value_bool(v[1]);
    *vret = bool_Value(b);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static RefNode *function_get_nodeptr(Value v)
{
    Ref *r = Value_ref(v);

    if (r->rh.n_memb > 0) {
        return Value_vp(r->v[INDEX_FUNC_FN]);
    } else {
        return Value_vp(v);
    }
}
static int function_tostr(Value *vret, Value *v, RefNode *node)
{
    RefNode *fn_n = function_get_nodeptr(*v);

    if (fn_n->u.f.klass != NULL && fn_n->u.f.klass->type == NODE_CLASS) {
        RefNode *cls_n = fn_n->u.f.klass;
        if (fn_n->type == NODE_NEW || fn_n->type == NODE_NEW_N) {
            *vret = printf_Value("%n.%n", cls_n, fn_n);
        } else {
            *vret = printf_Value("%n#%n", cls_n, fn_n);
        }
    } else {
        *vret = vp_Value(fn_n->name);
    }
    return TRUE;
}
/**
 * let f = function...
 * f.op_call()
 */
static int function_call(Value *vret, Value *v, RefNode *node)
{
    int argc = fg->stk_top - v - 1;

    if (!call_function_obj(argc)) {
        return FALSE;
    }
    *vret = *v;
    *v = VALUE_NULL;

    return TRUE;
}
/**
 * 配列を展開して引数に渡す
 * f(1, 2, 3)
 * f.call([1, 2, 3])
 */
static int function_call_list(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(v[1]);

    if ((fg->stk_top + ra->size) - fg->stk_base > fs->max_stack) {
        throw_errorf(fs->mod_lang, "StackOverflowError", "Stack overflow");
        return FALSE;
    }
    if (ra->size > 0) {
        memcpy(v + 1, ra->p, sizeof(Value) * ra->size);
    }
    fg->stk_top = v + 1 + ra->size;
    free(ra->p);

    if (!call_function_obj(ra->size)) {
        return FALSE;
    }
    *vret = *v;
    *v = VALUE_NULL;

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int lang_ref_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_ref = FUNC_VP(node);
    Ref *r = ref_new(cls_ref);

    *vret = vp_Value(r);
    r->v[0] = v[1];
    v[1] = VALUE_NULL;

    return TRUE;
}
static int lang_ref_tohash(Value *vret, Value *v, RefNode *node)
{
    Ref *r1 = Value_ref(*v);
    int32_t hash = int32_hash(Value_integral(r1->v[0]));
    *vret = int32_Value(hash);
    return TRUE;
}
static int lang_ref_eq(Value *vret, Value *v, RefNode *node)
{
    Ref *r1 = Value_ref(*v);
    Ref *r2 = Value_ref(v[1]);
    *vret = bool_Value(r1->v[0] == r2->v[0]);
    return TRUE;
}
static int weakref_new(Value *vret, Value *v, RefNode *node)
{
    Ref *ref;
    Value v1 = v[1];

    if (Value_isref(v1)) {
        RefHeader *r1 = Value_ref_header(v1);
        Ref *r = r1->weak_ref;
        if (r != NULL) {
            // すでに存在する場合は再利用
            ref = r;
        } else {
            ref = ref_new_n(cls_weakref, INDEX_WEAKREF_NUM);
            ref->v[0] = v1;
            ref->v[1] = VALUE_TRUE;
            ref->rh.nref++;
            r1->weak_ref = ref;
        }
    } else {
        // 参照ではない型は、毎回作成する
        ref = ref_new_n(cls_weakref, INDEX_WEAKREF_NUM);
        ref->v[0] = v1;
        ref->v[1] = VALUE_TRUE;
    }
    ref->rh.nref++;
    *vret = vp_Value(ref);

    return TRUE;
}

static int weakref_alive(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int inv = FUNC_INT(node);
    int ret = Value_bool(r->v[1]);
    if (inv) {
        ret = !ret;
    }
    *vret = bool_Value(ret);

    return TRUE;
}
static int weakref_value(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    if (Value_bool(r->v[INDEX_WEAKREF_ALIVE])) {
        *vret = Value_cp(r->v[INDEX_WEAKREF_REF]);
    } else {
        throw_errorf(fs->mod_lang, "WeakRefError", "Reference is not alive");
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int module_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *nm = Value_vp(v[1]);
    RefNode *mod = get_module_by_name(nm->c, nm->size, FALSE, TRUE);

    if (mod == NULL) {
        return FALSE;
    }
    *vret = vp_Value(mod);
    return TRUE;
}
static int module_tostr(Value *vret, Value *v, RefNode *node)
{
    RefNode *mod = Value_vp(*v);
    *vret = vp_Value(mod->name);
    return TRUE;
}

static int get_module_child(Value *vret, Value *v, RefNode *node)
{
    int is_func = FUNC_INT(node);
    RefNode *v1_type = Value_type(v[1]);
    RefNode *mod;
    RefNode *ret;
    RefStr *name;

    if (v1_type == fs->cls_str) {
        RefStr *nm = Value_vp(v[1]);
        mod = get_module_by_name(nm->c, nm->size, FALSE, TRUE);
        if (mod == NULL) {
            return FALSE;
        }
    } else if (v1_type == fs->cls_module) {
        mod = Value_vp(v[1]);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_module, v1_type, 1);
        return FALSE;
    }
    name = Value_vp(v[2]);

    // privateスコープにはアクセスできない
    if (name->size > 0 && name->c[0] == '_') {
        throw_errorf(fs->mod_lang, "DefineError", "%r is private", name);
        return FALSE;
    }

    ret = Hash_get(&mod->u.c.h, name->c, name->size);
    if (is_func) {
        if (ret == NULL || (ret->type != NODE_FUNC && ret->type != NODE_FUNC_N)) {
            throw_errorf(fs->mod_lang, "NameError", "%n has no function named %r", mod, name);
            return FALSE;
        }
    } else {
        if (ret == NULL || ret->type != NODE_CLASS) {
            throw_errorf(fs->mod_lang, "NameError", "%n has no class named %r", mod, name);
            return FALSE;
        }
    }
    *vret = vp_Value(ret);

    return TRUE;
}
static int cls_tostr(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls = Value_vp(*v);
    RefNode *mod = cls->defined_module;
    *vret = printf_Value("%n.%n", mod, cls);
    return TRUE;
}
/**
 * SubClass in SuperClass
 * SuperClass.__in(SubClass)
 */
static int cls_in(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls = Value_vp(*v);
    RefNode *cls2 = Value_vp(v[1]);

    for (;;) {
        if (cls2 == cls) {
            *vret = VALUE_TRUE;
            break;
        }
        if (cls2 == fs->cls_obj) {
            *vret = VALUE_FALSE;
            break;
        }
        cls2 = cls2->u.c.super;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int error_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = FUNC_VP(node);
    Ref *r = Value_ref(*v);

    if (*v == VALUE_NULL || r->rh.n_memb == 0) {
        r = ref_new(type);
        *vret = vp_Value(r);
    }

    if (fg->stk_top > v + 1) {
        r->v[INDEX_ERROR_MESSAGE] = Value_cp(v[1]);
    } else {
        r->v[INDEX_ERROR_MESSAGE] = vp_Value(fs->str_0);
    }

    return TRUE;
}
static int error_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefNode *type = r->rh.type;
    RefStr *msg = Value_vp(r->v[INDEX_ERROR_MESSAGE]);

    if (msg != NULL && msg->size > 0) {
        *vret = printf_Value("%n:%r", type, msg);
    } else {
        *vret = vp_Value(type->name);
    }
    return TRUE;
}
static int error_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    PtrList *pl = Value_ptr(r->v[INDEX_ERROR_STACKTRACE]);
    PtrList_close(pl);
    r->v[INDEX_ERROR_STACKTRACE] = VALUE_NULL;

    return TRUE;
}
static void stacktrace_str_line(StrBuf *buf, StackTrace *t)
{
    if (t->module != NULL) {
        StrBuf_add(buf, t->module->u.m.src_path, -1);
    } else {
        StrBuf_add(buf, "[system]", -1);
    }
    if (t->func != NULL) {
        RefNode *klass = t->func->u.f.klass;
        RefStr *name = t->func->name;
        StrBuf_add_c(buf, ':');

        if (klass != NULL && klass->type == NODE_CLASS) {
            StrBuf_add_r(buf, klass->name);
            if (t->func->type == NODE_NEW || t->func->type == NODE_NEW_N) {
                StrBuf_add_c(buf, '.');
            } else {
                StrBuf_add_c(buf, '#');
            }
            StrBuf_add_r(buf, name);
        } else {
            StrBuf_add_r(buf, name);
        }
    }
    if (t->line > 0) {
        char cbuf[16];
        sprintf(cbuf, "(%d)", t->line);
        StrBuf_add(buf, cbuf, -1);
    }
}
void stacktrace_to_str(StrBuf *buf, Value v, char sep)
{
    Ref *r = Value_ref(v);
    RefNode *type = r->rh.type;
    PtrList *p = Value_ptr(r->v[INDEX_ERROR_STACKTRACE]);

    /*
     * stacktrace処理中は、エラー処理の最中なので、
     * メモリ不足エラーを投げても処理できない可能性が高い
     */
    StrBuf_add_r(buf, type->name);
    StrBuf_add_c(buf, ':');
    {
        RefStr *rs = Value_vp(r->v[INDEX_ERROR_MESSAGE]);
        if (rs != NULL) {
            StrBuf_add_r(buf, rs);
        }
    }
    StrBuf_add_c(buf, sep);

    for (; p != NULL; p = p->next) {
        stacktrace_str_line(buf, (StackTrace*)p->u.c);
        if (p->next != NULL) {
            StrBuf_add_c(buf, sep);
        }
    }
    StrBuf_add_c(buf, '\n');
}
static void stacktrace_to_array(RefArray *ra, Value v)
{
    Ref *r = Value_ref(v);
    PtrList *p = Value_ptr(r->v[INDEX_ERROR_STACKTRACE]);

    for (; p != NULL; p = p->next) {
        Value *vp = refarray_push(ra);
        StrBuf sb;
        StrBuf_init_refstr(&sb, 16);
        stacktrace_str_line(&sb, (StackTrace*)p->u.c);
        *vp = StrBuf_str_Value(&sb, fs->cls_str);
    }
}
static int error_stacktrace(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = refarray_new(0);
    *vret = vp_Value(ra);
    stacktrace_to_array(ra, *v);

    return TRUE;
}
static int error_message(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = Value_cp(r->v[INDEX_ERROR_MESSAGE]);
    return TRUE;
}
/**
 * 例外送出中にtraceに追加する
 */
void add_stack_trace(RefNode *module, RefNode *func, int line)
{
    StackTrace *t;

    if (fg->error == VALUE_NULL) {
        fatal_errorf("add_stack_trace while throwing no errors at %n", func);
        return;
    } else {
        Ref *r = Value_ref(fg->error);
        PtrList *p = Value_ptr(r->v[INDEX_ERROR_STACKTRACE]);
        PtrList *pl = PtrList_push(&p, sizeof(StackTrace), NULL);

        r->v[INDEX_ERROR_STACKTRACE] = ptr_Value(p);
        t = (StackTrace*)pl->u.c;
    }

    if (module == NULL && func != NULL) {
        t->module = func->defined_module;
    } else {
        t->module = module;
    }
    t->func = func;
    t->line = line;
}

////////////////////////////////////////////////////////////////////////////////

static int lang_env(Value *vret, Value *v, RefNode *node)
{
    int i;
    RefMap *rm = refmap_new(fs->envs.count);
    *vret = vp_Value(rm);

    for (i = 0; i < fs->envs.entry_num; i++) {
        HashEntry *he = fs->envs.entry[i];
        for (; he != NULL; he = he->next) {
            HashValueEntry *ve;
            RefStr *rkey = he->key;
            ve = refmap_add(rm, vp_Value(rkey), FALSE, FALSE);
            if (ve == NULL) {
                return FALSE;
            }
            ve->val = cstr_Value(NULL, he->p, -1);
        }
    }

    return TRUE;
}
static int lang_argv(Value *vret, Value *v, RefNode *node)
{
    int i;
    RefArray *ar = refarray_new(fv->argc);
    *vret = vp_Value(ar);

    for (i = 0; i < fv->argc; i++) {
        ar->p[i] = cstr_Value(NULL, fv->argv[i], -1);
    }
    return TRUE;
}
static void refmap_add_str(RefMap *rm, const char *key, Value val)
{
    HashValueEntry *ve = refmap_add(rm, vp_Value(intern(key, -1)), FALSE, FALSE);
    if (ve != NULL) {
        ve->val = val;
    }
}
static int lang_prop(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = refmap_new(5);
    *vret = vp_Value(rm);

#ifdef WIN32
    refmap_add_str(rm, "platform", vp_Value(intern("WIN", -1)));
#elif defined(APPLE)
    refmap_add_str(rm, "platform", vp_Value(intern("OSX", -1)));
#else
    refmap_add_str(rm, "platform", vp_Value(intern("POSIX", -1)));
#endif
    refmap_add_str(rm, "path_separator", vp_Value(intern(SEP_S, -1)));

    switch (fs->running_mode) {
    case RUNNING_MODE_CL:
        refmap_add_str(rm, "mode", vp_Value(intern("CL", -1)));
        break;
    case RUNNING_MODE_CGI:
        refmap_add_str(rm, "mode", vp_Value(intern("CGI", -1)));
        break;
    case RUNNING_MODE_GUI:
        refmap_add_str(rm, "mode", vp_Value(intern("GUI", -1)));
        break;
    }

    refmap_add_str(rm, "home", vp_Value(fs->fox_home));

    {
        RefArray *ra = refarray_new(3);
        ra->p[0] = int32_Value(FOX_VERSION_MAJOR);
        ra->p[1] = int32_Value(FOX_VERSION_MINOR);
        ra->p[2] = int32_Value(FOX_VERSION_REVISION);
        refmap_add_str(rm, "version", vp_Value(ra));
    }

    refmap_add_str(rm, "arch_bits", int32_Value((int)sizeof(void*) * 8));

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

void define_error_class(RefNode *cls, RefNode *base, RefNode *m)
{
    RefNode *n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, error_new, 0, 1, cls, fs->cls_str);
    cls->u.c.n_memb = INDEX_ERROR_NUM;
    cls->u.c.super = base;
    extends_method(cls, base);
}
void define_uncaught_class(RefNode *cls, RefNode *base, RefNode *m)
{
    cls->u.c.n_memb = INDEX_ERROR_NUM;
    extends_method(cls, base);
}
static void define_lang_func(RefNode *m)
{
    RefNode *n;

    // 型を取得
    n = define_identifier(m, m, "typeof", NODE_FUNC_N, 0);
    define_native_func_a(n, lang_typeof, 1, 1, NULL, NULL);

    // 参照カウントを取得
    n = define_identifier(m, m, "ref_count", NODE_FUNC_N, 0);
    define_native_func_a(n, lang_ref_count, 1, 1, NULL, NULL);

    // ヒープオブジェクトの数を取得
    n = define_identifier(m, m, "heap_count", NODE_FUNC_N, 0);
    define_native_func_a(n, lang_heap_count, 0, 0, NULL);
}
static void define_lang_const(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "ENV", NODE_CONST_U_N, 0);
    define_native_func_a(n, lang_env, 0, 0, NULL);

    n = define_identifier(m, m, "ARGV", NODE_CONST_U_N, 0);
    define_native_func_a(n, lang_argv, 0, 0, NULL);

    n = define_identifier(m, m, "PROP", NODE_CONST_U_N, 0);
    define_native_func_a(n, lang_prop, 0, 0, NULL);
}

static void define_lang_class(RefNode *m)
{
    RefNode *n;
    RefNode *cls, *cls2;
    RefStr *empty = intern("empty", -1);

    // Object
    cls = fs->cls_obj;
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, object_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, native_return_bool, 0, 0, (void*)FALSE);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, object_hash, 0, 0, NULL);

    // Null
    cls = fs->cls_null;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, native_return_null, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, native_return_null, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, null_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, native_return_bool, 0, 0, (void*)TRUE);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_bool, 1, 1, (void*)TRUE, fs->cls_null);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_null, 1, 1, NULL, fs->cls_marshaldumper);
    extends_method(cls, fs->cls_obj);

    // Bool
    cls = fs->cls_bool;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, bool_new, 0, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, bool_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, bool_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, bool_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);

    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, bool_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    extends_method(cls, fs->cls_obj);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_AND], NODE_FUNC_N, 0);
    define_native_func_a(n, bool_and, 1, 1, NULL, fs->cls_bool);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_OR], NODE_FUNC_N, 0);
    define_native_func_a(n, bool_or, 1, 1, NULL, fs->cls_bool);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_XOR], NODE_FUNC_N, 0);
    define_native_func_a(n, bool_xor, 1, 1, NULL, fs->cls_bool);

    // Module
    cls = fs->cls_module;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, module_new, 1, 1, NULL, fs->cls_str);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, module_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, object_eq, 1, 1, NULL, fs->cls_class);
    extends_method(cls, fs->cls_obj);

    // Class
    cls = fs->cls_class;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, get_module_child, 2, 2, (void*)FALSE, NULL, fs->cls_str);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, cls_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, object_eq, 1, 1, NULL, fs->cls_class);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, cls_in, 1, 1, NULL, fs->cls_class);
    extends_method(cls, fs->cls_obj);

    // Function
    cls = fs->cls_fn;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, get_module_child, 2, 2, (void*)TRUE, NULL, fs->cls_str);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, function_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LP], NODE_FUNC_N, 0);
    define_native_func_a(n, function_call, 0, -1, NULL);
    n = define_identifier(m, cls, "call", NODE_FUNC_N, 0);
    define_native_func_a(n, function_call_list, 1, 1, NULL, fs->cls_list);
    extends_method(cls, fs->cls_obj);

    // Ref
    cls = define_identifier(m, m, "Ref", NODE_CLASS, 0);
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, lang_ref_new, 1, 1, cls, NULL);

    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, lang_ref_tohash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, lang_ref_eq, 1, 1, NULL, cls);

    n = define_identifier(m, cls, "value", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, native_get_member, 0, 0, (void*) 0);
    n = define_identifier(m, cls, "value=", NODE_FUNC_N, 0);
    define_native_func_a(n, native_set_member, 1, 1, (void*) 0, NULL);
    cls->u.c.n_memb = 1;
    extends_method(cls, fs->cls_obj);

    // WeakRef
    cls = define_identifier(m, m, "WeakRef", NODE_CLASS, 0);
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, weakref_new, 1, 1, cls, NULL);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, object_eq, 1, 1, NULL, cls);

    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, weakref_alive, 0, 0, (void*)TRUE);
    n = define_identifier(m, cls, "alive", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, weakref_alive, 0, 0, (void*)FALSE);
    n = define_identifier(m, cls, "value", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, weakref_value, 0, 0, NULL);
    // GC対象外とするため、0を設定
    cls->u.c.n_memb = 0;
    extends_method(cls, fs->cls_obj);

    // Error
    cls = fs->cls_error;
    cls->u.c.n_memb = INDEX_ERROR_NUM;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, error_new, 0, 1, cls, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, error_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    define_native_func_a(n, error_dispose, 0, 0, NULL);

    n = define_identifier(m, cls, "stack_trace", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, error_stacktrace, 0, 0, NULL);
    n = define_identifier(m, cls, "message", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, error_message, 0, 0, NULL);
    extends_method(cls, fs->cls_obj);


    // UncaughtError
    cls = fs->cls_uncaught;
    define_uncaught_class(cls, fs->cls_obj, m);


    cls = define_identifier(m, m, "TypeError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);

    cls = define_identifier(m, m, "NameError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);

    cls = define_identifier(m, m, "WeakRefError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);

    cls = define_identifier(m, m, "IlligalOperationError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);

    cls = define_identifier(m, m, "SwitchError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
    fs->cls_switcherror = cls;

    cls = define_identifier(m, m, "StopIteration", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
    fs->cls_stopiter = cls;

    cls = define_identifier(m, m, "ValueError", NODE_CLASS, NODEOPT_ABSTRACT);
    define_error_class(cls, fs->cls_error, m);
    cls2 = cls;

    cls = define_identifier(m, m, "FormatError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "ParseError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "LoopReferenceError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "NotSupportedError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "NotImplementedError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);


    cls = define_identifier(m, m, "ArgumentError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);

    cls = define_identifier(m, m, "StackOverflowError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);

    cls = define_identifier(m, m, "IndexError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);

    cls = define_identifier(m, m, "MemoryError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);


    cls = define_identifier(m, m, "ArithmeticError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
    cls2 = cls;

    cls = define_identifier(m, m, "ZeroDivisionError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "FloatError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);


    cls = define_identifier(m, m, "OutOfMemoryError", NODE_CLASS, NODEOPT_ABSTRACT);
    define_uncaught_class(cls, fs->cls_uncaught, m);

    cls = define_identifier(m, m, "CompileError", NODE_CLASS, NODEOPT_ABSTRACT);
    define_uncaught_class(cls, fs->cls_uncaught, m);
    cls2 = cls;

    cls = define_identifier(m, m, "TokenError", NODE_CLASS, 0);
    define_uncaught_class(cls, cls2, m);

    cls = define_identifier(m, m, "SyntaxError", NODE_CLASS, 0);
    define_uncaught_class(cls, cls2, m);

    cls = define_identifier(m, m, "DefineError", NODE_CLASS, 0);
    define_uncaught_class(cls, cls2, m);

    cls = define_identifier(m, m, "ImportError", NODE_CLASS, 0);
    define_uncaught_class(cls, cls2, m);

    cls = define_identifier(m, m, "TypeNameError", NODE_CLASS, 0);
    define_uncaught_class(cls, cls2, m);

    cls = define_identifier(m, m, "InternalError", NODE_CLASS, 0);
    define_uncaught_class(cls, cls2, m);
}

void init_lang_module_stubs()
{
    RefNode *m = Module_new_sys("lang");

    redefine_identifier(m, m, "Str", NODE_CLASS, NODEOPT_STRCLASS, fs->cls_str);
    redefine_identifier(m, m, "Class", NODE_CLASS, 0, fs->cls_class);
    redefine_identifier(m, m, "Function", NODE_CLASS, 0, fs->cls_fn);
    redefine_identifier(m, m, "Module", NODE_CLASS, 0, fs->cls_module);

    fs->cls_obj = define_identifier(m, m, "Object", NODE_CLASS, NODEOPT_ABSTRACT);
    fs->cls_null = define_identifier(m, m, "Null", NODE_CLASS, 0);
    fs->cls_bool = define_identifier(m, m, "Bool", NODE_CLASS, NODEOPT_INTEGRAL);
    fs->cls_number = define_identifier(m, m, "Number", NODE_CLASS, NODEOPT_ABSTRACT);
    fs->cls_int = define_identifier(m, m, "Int", NODE_CLASS, NODEOPT_INTEGRAL);
    fs->cls_frac = define_identifier(m, m, "Frac", NODE_CLASS, 0);
    fs->cls_float = define_identifier(m, m, "Float", NODE_CLASS, 0);
    fs->cls_sequence = define_identifier(m, m, "Sequence", NODE_CLASS, NODEOPT_ABSTRACT);
    fs->cls_bytes = define_identifier(m, m, "Bytes", NODE_CLASS, NODEOPT_STRCLASS);
    fs->cls_regex = define_identifier(m, m, "Regex", NODE_CLASS, 0);
    fs->cls_list = define_identifier(m, m, "List", NODE_CLASS, 0);
    fs->cls_map = define_identifier(m, m, "Map", NODE_CLASS, 0);
    fs->cls_set = define_identifier(m, m, "Set", NODE_CLASS, 0);
    fs->cls_range = define_identifier(m, m, "Range", NODE_CLASS, 0);
    fs->cls_charset = define_identifier(m, m, "Charset", NODE_CLASS, 0);

    cls_weakref = define_identifier(m, m, "WeakRef", NODE_CLASS, 0);
    fs->cls_iterable = define_identifier(m, m, "Iterable", NODE_CLASS, NODEOPT_ABSTRACT);
    fs->cls_iterator = define_identifier(m, m, "Iterator", NODE_CLASS, NODEOPT_ABSTRACT);
    fv->cls_generator = define_identifier(m, m, "Generator", NODE_CLASS, 0);

    fs->cls_error = define_identifier(m, m, "Error", NODE_CLASS, NODEOPT_ABSTRACT);
    fs->cls_uncaught = define_identifier(m, m, "UncaughtError", NODE_CLASS, NODEOPT_ABSTRACT);

    fs->mod_lang = m;
}
void init_lang_module_1()
{
    RefNode *m = fs->mod_lang;

    define_lang_func(m);
    define_charset_class(m);
    define_lang_str_func(m);
    define_lang_class(m);
    define_lang_number_class(m);
    define_lang_number_func(m);
    define_lang_col_class(m);
    define_lang_map_class(m);
    define_lang_str_class(m);
    define_lang_const(m);

    m->u.m.loaded = TRUE;
}
