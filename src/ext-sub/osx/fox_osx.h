#ifndef _FOX_OSX_H_
#define _FOX_OSX_H_

#include "fox.h"


#ifdef DEFINE_GLOBALS
#define extern
#endif
extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *mod_osx;
#ifdef DEFINE_GLOBALS
#undef extern
#endif



#endif /* _FOX_OSX_H_ */
