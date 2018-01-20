#define DEFINE_GLOBALS
#include "fox_macos.h"
#include <stdio.h>
#include <stdlib.h>


static int applescript_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_as = FUNC_VP(node);
    RefAppleScript *r = fs->buf_new(cls_as, sizeof(RefAppleScript));
    const char *src = Value_cstr(v[1]);

    *vret = vp_Value(r);
    if (!apple_script_new_sub(r, src)) {
        return FALSE;
    }
    return TRUE;
}
static int applescript_close(Value *vret, Value *v, RefNode *node)
{
    RefAppleScript *r = Value_vp(*v);
    apple_script_close_sub(r);
    return TRUE;
}
static int applescript_exec(Value *vret, Value *v, RefNode *node)
{
    RefAppleScript *r = Value_vp(*v);

    if (!apple_script_exec_sub(r)) {
        return FALSE;
    }
    return TRUE;
}

static int output_nslog(Value *vret, Value *v, RefNode *node)
{
    output_nslog_sub(Value_cstr(v[1]));
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;
    n = fs->define_identifier(m, m, "debug_log", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, output_nslog, 1, 1, NULL, fs->cls_str);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;
    RefNode *cls_applescript;

    cls_applescript = fs->define_identifier(m, m, "AppleScript", NODE_CLASS, 0);

    cls = cls_applescript;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, applescript_new, 1, 1, cls_applescript, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, applescript_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "exec", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, applescript_exec, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "AppleScriptError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_macos = m;

    define_func(m);
    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    if (buf == NULL) {
        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\n");
    }
    return buf;
}
