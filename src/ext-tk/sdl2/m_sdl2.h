#ifndef _M_SDL2_H_
#define _M_SDL2_H_

#include "fox.h"
#include <SDL.h>
#include <SDL_opengl.h>


typedef struct {
    RefHeader rh;
    SDL_Window *w;
} RefSDLWindow;

typedef struct {
    RefHeader rh;
    SDL_GLContext *gl;
} RefGLContext;

typedef struct {
    RefHeader rh;
    SDL_Surface *sf;
} RefSDLSurface;


#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *mod_sdl;

extern RefNode *cls_window;
extern RefNode *cls_surface;
extern RefNode *cls_glcontext;
extern RefNode *cls_vector;
extern RefNode *cls_matrix;

extern RefSDLWindow *gl_cur_window;
extern RefGLContext *gl_cur_context;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


void throw_sdl_error(void);
void throw_already_closed(Value v);


// sdl_gl.c
void define_gl_func(RefNode *m);
void define_gl_class(RefNode *m);

#endif /* _M_SDL2_H_ */
