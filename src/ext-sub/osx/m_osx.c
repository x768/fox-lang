#define DEFINE_GLOBALS
#include "fox_osx.h"
#include <stdio.h>
#include <stdlib.h>



////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
    //RefNode *cls;
    //RefNode *n;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_osx = m;

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
