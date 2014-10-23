#ifndef _M_SDL2_H_
#define _M_SDL2_H_

#include "fox.h"
#include <SDL.h>


typedef struct {
    RefHeader rh;
    SDL_Window *w;
} RefSDLWindow;

typedef struct {
    RefHeader rh;
    SDL_Surface *sf;
} RefSDLSurface;

typedef struct {
    RefHeader rh;
    SDL_Event e;
} RefSDLEvent;


void throw_sdl_error(void);
void throw_already_closed(Value v);

#endif /* _M_SDL2_H_ */
