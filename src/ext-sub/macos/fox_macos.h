#ifndef FOX_MACOS_H_INCLUDED
#define FOX_MACOS_H_INCLUDED

#include "fox.h"


typedef struct {
    RefHeader rh;
    void *apple_script;
} RefAppleScript;


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
int apple_script_new_sub(RefAppleScript *r, const char *src);
void apple_script_close_sub(RefAppleScript *r);
int apple_script_exec_sub(RefAppleScript *r);
void output_nslog_sub(const char *src);


#endif /* FOX_MACOS_H_INCLUDED */
