#ifndef FOX_WINDOWS_H_INCLUDED
#define FOX_WINDOWS_H_INCLUDED


#define UNICODE
#define WINVER 0x500

#include "fox.h"
#include "fox_io.h"
#include "m_number.h"
#include <string.h>
#include <limits.h>

#ifndef WIN32
#error This module is only for win32
#endif

#include <windows.h>


#ifndef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_windows;
extern RefNode *cls_oleobject;
extern RefTimeZone *default_tz;

#ifndef DEFINE_GLOBALS
#undef extern
#endif

enum {
    INDEX_CODEPAGEIO_CODEPAGE = INDEX_TEXTIO_NUM,
    INDEX_CODEPAGEIO_NUM,
};
enum {
    TEXTIO_M_GETS,
    TEXTIO_M_GETLN,
    TEXTIO_M_NEXT,
};

void throw_windows_error(const char *err_class, DWORD errcode);

int ole_new(Value *vret, Value *v, RefNode *node);
int ole_close(Value *vret, Value *v, RefNode *node);
int ole_invoke_method(Value *vret, Value *v, RefNode *node);
int ole_get_iterator(Value *vret, Value *v, RefNode *node);
int oleenum_next(Value *vret, Value *v, RefNode *node);
int oleenum_close(Value *vret, Value *v, RefNode *node);


int codepageio_new(Value *vret, Value *v, RefNode *node);
int codepageio_gets(Value *vret, Value *v, RefNode *node);
int codepageio_write(Value *vret, Value *v, RefNode *node);
int codepageio_codepage(Value *vret, Value *v, RefNode *node);
int cpio_tobytes(Value *vret, Value *v, RefNode *node);
int cpio_tostr(Value *vret, Value *v, RefNode *node);

#endif /* FOX_WINDOWS_H_INCLUDED */
