#include "fox.h"
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_charset;
static RefNode *cls_charset;


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
//    RefNode *n;

    cls_charset = fs->define_identifier(m, m, "Charset", NODE_CLASS, 0);


    cls = cls_charset;
//    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);

    fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_charset = m;

    define_func(m);
    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\n";
}

