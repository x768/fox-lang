#ifndef FOX_MACOS_H_INCLUDED
#define FOX_MACOS_H_INCLUDED

#include "fox.h"


#ifdef DEFINE_GLOBALS
#define extern
#endif
extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *mod_macos;
#ifdef DEFINE_GLOBALS
#undef extern
#endif

int exec_apple_script(const char *src);

#endif /* FOX_MACOS_H_INCLUDED */
