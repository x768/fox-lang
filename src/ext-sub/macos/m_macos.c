#define DEFINE_GLOBALS
#include "fox_macos.h"
#include <stdio.h>
#include <stdlib.h>


static int macos_exec_apple_script(Value *vret, Value *v, RefNode *node)
{
    const char *src = Value_cstr(v[1]);
    if (!exec_apple_script(src)) {
        // ...
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;
    
    n = fs->define_identifier(m, m, "exec_apple_script", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, macos_exec_apple_script, 1, 1, NULL, fs->cls_str);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    //RefNode *n;

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
