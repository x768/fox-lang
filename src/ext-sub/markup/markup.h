#ifndef MARKUP_H_INCLUDED
#define MARKUP_H_INCLUDED

#include "fox.h"

#ifndef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *mod_markup;

#ifndef DEFINE_GLOBALS
#undef extern
#endif

#endif /* MARKUP_H_INCLUDED */
