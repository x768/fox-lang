#define DEFINE_GLOBALS
#include "m_sdl2.h"
#include <stdio.h>
#include <stdlib.h>


typedef struct
{
	RefStr *which;
	RefStr *button;
	RefStr *state;
	RefStr *repeat;
	RefStr *x, *y;
	RefStr *xrel, *yrel;
} SDLEventID;


static const FoxStatic *fs;
static FoxGlobal *fg;
static RefNode *mod_sdl;

static RefNode *cls_window;
static RefNode *cls_surface;


void throw_sdl_error()
{
	fs->throw_errorf(mod_sdl, "SDLError", "%s", SDL_GetError());
}
void throw_already_closed(Value v)
{
	const RefNode *type = fs->Value_type(v);
	fs->throw_errorf(fs->mod_lang, "ValueError", "%r already closed", type);
}
static void throw_no_member(Value v, RefStr *name)
{
	const RefNode *type = fs->Value_type(v);
	fs->throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, type, name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sdl_init(Value *vret, Value *v, RefNode *node)
{
	int err = FALSE;
	int flags = fs->Value_int(v[1], &err);

	if (err) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "argument #1 out of range");
		return FALSE;
	}
	if (SDL_Init(flags) != 0) {
		throw_sdl_error();
		return FALSE;
	}
	return TRUE;
}
static int sdl_dispose(Value *vret, Value *v, RefNode *node)
{
	SDL_Quit();
	return TRUE;
}

static int sdl_delay(Value *vret, Value *v, RefNode *node)
{
	int err = FALSE;
	int ms = fs->Value_int(v[1], &err);

	if (err) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "argument #1 out of range");
		return FALSE;
	}
	SDL_Delay(ms);
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sdlwindow_new(Value *vret, Value *v, RefNode *node)
{
	RefSDLWindow *window;
	int err[5] = {0};
	int i;

	RefStr *title = Value_vp(v[1]);
	int x = fs->Value_int(v[2], &err[0]);
	int y = fs->Value_int(v[3], &err[1]);
	int w = fs->Value_int(v[4], &err[2]);
	int h = fs->Value_int(v[5], &err[3]);
	int flags = 0;

	if (fg->stk_top > v + 6) {
		flags = fs->Value_int(v[6], &err[4]);
	}

	for (i = 0; i < lengthof(err); i++) {
		if (err[i]) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "argument #%d out of range", i + 2);
			return FALSE;
		}
	}
	window = fs->buf_new(cls_window, sizeof(RefSDLWindow));
	*vret = vp_Value(window);

	window->w = SDL_CreateWindow(title->c, x, y, w, h, flags);
	if (window->w == NULL) {
		throw_sdl_error();
		return FALSE;
	}

	return TRUE;
}
static int sdlwindow_close(Value *vret, Value *v, RefNode *node)
{
	RefSDLWindow *window = Value_vp(*v);

	if (window->w != NULL) {
		SDL_DestroyWindow(window->w);
		window->w = NULL;
	}

	return TRUE;
}
static int sdlwindow_get_title(Value *vret, Value *v, RefNode *node)
{
	RefSDLWindow *window = Value_vp(*v);

	if (window->w == NULL) {
		throw_already_closed(*v);
		return FALSE;
	}
	*vret = fs->cstr_Value(fs->cls_str, SDL_GetWindowTitle(window->w), -1);

	return TRUE;
}
static int sdlwindow_set_title(Value *vret, Value *v, RefNode *node)
{
	RefSDLWindow *window = Value_vp(*v);
	RefStr *str = Value_vp(v[1]);

	if (window->w == NULL) {
		throw_already_closed(*v);
		return FALSE;
	}
	SDL_SetWindowTitle(window->w, str->c);

	return TRUE;
}
static int sdlwindow_get_surface(Value *vret, Value *v, RefNode *node)
{
	RefSDLWindow *window = Value_vp(*v);
	RefSDLSurface *sf;
	SDL_Surface *s;

	if (window->w == NULL) {
		throw_already_closed(*v);
		return FALSE;
	}
	s = SDL_GetWindowSurface(window->w);
	if (s == NULL) {
		throw_sdl_error();
		return FALSE;
	}
	sf = fs->buf_new(cls_surface, sizeof(RefSDLSurface));
	sf->sf = s;

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sdlevent_new(Value *vret, Value *v, RefNode *node)
{
	RefNode *cls_event = FUNC_VP(node);
	RefSDLEvent *evt = fs->buf_new(cls_event, sizeof(RefSDLEvent));

	*vret = vp_Value(evt);
	return TRUE;
}
static int sdlevent_wait(Value *vret, Value *v, RefNode *node)
{
	RefSDLEvent *evt = Value_vp(*v);
	if (SDL_WaitEvent(&evt->e) == 0) {
		throw_sdl_error();
		return FALSE;
	}
	return TRUE;
}
static int sdlevent_poll(Value *vret, Value *v, RefNode *node)
{
	RefSDLEvent *evt = Value_vp(*v);
	int ret = SDL_PollEvent(&evt->e);
	*vret = bool_Value(ret);
	return TRUE;
}
static int sdlevent_type(Value *vret, Value *v, RefNode *node)
{
	RefSDLEvent *evt = Value_vp(*v);
	*vret = int32_Value(evt->e.type);
	return TRUE;
}

static SDLEventID *init_sdlevent_id(void)
{
#define DEFINE_EID(name) ((e->name) = fs->intern(#name, -1))
	SDLEventID *e = fs->Mem_get(&fg->st_mem, sizeof(SDLEventID));

	DEFINE_EID(which);
	DEFINE_EID(button);
	DEFINE_EID(state);
	DEFINE_EID(repeat);
	DEFINE_EID(x);
	DEFINE_EID(y);
	DEFINE_EID(xrel);
	DEFINE_EID(yrel);

	return e;
#undef DEFINE_EID
}
static int sdlevent_get(Value *vret, Value *v, RefNode *node)
{
	static SDLEventID *eid = NULL;

	RefSDLEvent *evt = Value_vp(*v);
	RefStr *name = Value_vp(v[1]);
	int result;

	if (eid == NULL) {
		eid = init_sdlevent_id();
	}

	switch (evt->e.type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		if (name == eid->state) {
			result = evt->e.key.state;
		} else if (name == eid->repeat) {
			result = evt->e.key.repeat;
		} else {
			goto ERROR_END;
		}
		break;
	case SDL_MOUSEMOTION:
		if (name == eid->which) {
			result = evt->e.motion.which;
		} else if (name == eid->state) {
			result = evt->e.motion.state;
		} else if (name == eid->x) {
			result = evt->e.motion.x;
		} else if (name == eid->y) {
			result = evt->e.motion.y;
		} else if (name == eid->xrel) {
			result = evt->e.motion.xrel;
		} else if (name == eid->yrel) {
			result = evt->e.motion.yrel;
		} else {
			goto ERROR_END;
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (name == eid->which) {
			result = evt->e.button.which;
		} else if (name == eid->button) {
			result = evt->e.button.button;
		} else if (name == eid->state) {
			result = evt->e.button.state;
		} else if (name == eid->x) {
			result = evt->e.button.x;
		} else if (name == eid->y) {
			result = evt->e.button.y;
		} else {
			goto ERROR_END;
		}
		break;
	case SDL_MOUSEWHEEL:
		if (name == eid->which) {
			result = evt->e.button.which;
		} else if (name == eid->x) {
			result = evt->e.button.x;
		} else if (name == eid->y) {
			result = evt->e.button.y;
		} else {
			goto ERROR_END;
		}
		break;
	default:
		goto ERROR_END;
	}
	*vret = int32_Value(result);

	return TRUE;

ERROR_END:
	throw_no_member(*v, name);
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sdlsurface_wh(Value *vret, Value *v, RefNode *node)
{
	RefSDLSurface *sf = Value_vp(*v);

	if (sf->sf != NULL) {
		int is_h = FUNC_INT(node);
		int32_t val = (is_h ? sf->sf->h : sf->sf->h);
		*vret = int32_Value(val);
	} else {
		throw_already_closed(*v);
		return FALSE;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_int(RefNode *m, const char *name, int32_t i)
{
	RefNode *n = fs->define_identifier(m, m, name, NODE_CONST, 0);
	n->u.k.val = int32_Value(i);
}
static void define_const(RefNode *m)
{
#define DEFINE_INT(s) define_int(m, #s, s)

	DEFINE_INT(SDL_INIT_TIMER);
	DEFINE_INT(SDL_INIT_AUDIO);
	DEFINE_INT(SDL_INIT_VIDEO);
	DEFINE_INT(SDL_INIT_JOYSTICK);
	DEFINE_INT(SDL_INIT_HAPTIC);
	DEFINE_INT(SDL_INIT_GAMECONTROLLER);
	DEFINE_INT(SDL_INIT_EVENTS);
	DEFINE_INT(SDL_INIT_EVERYTHING);

	DEFINE_INT(SDL_WINDOWPOS_CENTERED);
	DEFINE_INT(SDL_WINDOWPOS_UNDEFINED);

	DEFINE_INT(SDL_QUIT);
	DEFINE_INT(SDL_APP_TERMINATING);
	DEFINE_INT(SDL_APP_LOWMEMORY);
	DEFINE_INT(SDL_APP_WILLENTERBACKGROUND);
	DEFINE_INT(SDL_APP_DIDENTERBACKGROUND);
	DEFINE_INT(SDL_APP_WILLENTERFOREGROUND);
	DEFINE_INT(SDL_APP_DIDENTERFOREGROUND);
	DEFINE_INT(SDL_WINDOWEVENT);
	DEFINE_INT(SDL_SYSWMEVENT);
	DEFINE_INT(SDL_KEYDOWN);
	DEFINE_INT(SDL_KEYUP);
	DEFINE_INT(SDL_TEXTEDITING);
	DEFINE_INT(SDL_TEXTINPUT);
	DEFINE_INT(SDL_MOUSEMOTION);
	DEFINE_INT(SDL_MOUSEBUTTONDOWN);
	DEFINE_INT(SDL_MOUSEBUTTONUP);
	DEFINE_INT(SDL_MOUSEWHEEL);
	DEFINE_INT(SDL_JOYAXISMOTION);
	DEFINE_INT(SDL_JOYBALLMOTION);
	DEFINE_INT(SDL_JOYHATMOTION);
	DEFINE_INT(SDL_JOYBUTTONDOWN);
	DEFINE_INT(SDL_JOYBUTTONUP);
	DEFINE_INT(SDL_JOYDEVICEADDED);
	DEFINE_INT(SDL_JOYDEVICEREMOVED);
	DEFINE_INT(SDL_CONTROLLERAXISMOTION);
	DEFINE_INT(SDL_CONTROLLERBUTTONDOWN);
	DEFINE_INT(SDL_CONTROLLERBUTTONUP);
	DEFINE_INT(SDL_CONTROLLERDEVICEADDED);
	DEFINE_INT(SDL_CONTROLLERDEVICEREMOVED);
	DEFINE_INT(SDL_CONTROLLERDEVICEREMAPPED);
	DEFINE_INT(SDL_FINGERDOWN);
	DEFINE_INT(SDL_FINGERUP);
	DEFINE_INT(SDL_FINGERMOTION);
	DEFINE_INT(SDL_DOLLARGESTURE);
	DEFINE_INT(SDL_DOLLARRECORD);
	DEFINE_INT(SDL_MULTIGESTURE);
	DEFINE_INT(SDL_CLIPBOARDUPDATE);
	DEFINE_INT(SDL_DROPFILE);
	DEFINE_INT(SDL_USEREVENT);
	DEFINE_INT(SDL_LASTEVENT);


#undef DEFINE_INT
}

static void define_func(RefNode *m)
{
	RefNode *n;

	n = fs->define_identifier_p(m, m, fs->str_dispose, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdl_dispose, 0, 0, NULL);

	n = fs->define_identifier(m, m, "sdl_init", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdl_init, 1, 1, NULL, fs->cls_int);

	n = fs->define_identifier(m, m, "sdl_delay", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdl_delay, 1, 1, NULL, fs->cls_int);
}
static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

	cls_window = fs->define_identifier(m, m, "SDLWindow", NODE_CLASS, 0);
	cls_surface = fs->define_identifier(m, m, "SDLSurface", NODE_CLASS, 0);


	cls = cls_window;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, sdlwindow_new, 5, 6, NULL, fs->cls_str, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int, fs->cls_int);

	n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdlwindow_close, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "destroy", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdlwindow_close, 0, 0, NULL);

	n = fs->define_identifier(m, cls, "title", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, sdlwindow_get_title, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "title=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdlwindow_set_title, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier(m, cls, "surface", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, sdlwindow_get_surface, 0, 0, NULL);
	fs->extends_method(cls, fs->cls_obj);


	cls = fs->define_identifier(m, m, "SDLEvent", NODE_CLASS, 0);
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, sdlevent_new, 0, 0, cls);

	n = fs->define_identifier(m, cls, "poll", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdlevent_poll, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "wait", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdlevent_wait, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "type", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, sdlevent_type, 0, 0, NULL);
	n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, sdlevent_get, 1, 1, NULL, NULL);

	fs->extends_method(cls, fs->cls_obj);


	cls = cls_surface;
	n = fs->define_identifier(m, cls, "w", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, sdlsurface_wh, 0, 0, (void*)FALSE);
	n = fs->define_identifier(m, cls, "h", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, sdlsurface_wh, 0, 0, (void*)TRUE);
	fs->extends_method(cls, fs->cls_obj);


	cls = fs->define_identifier(m, m, "SDLError", NODE_CLASS, 0);
	cls->u.c.n_memb = 2;
	fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_sdl = m;

	define_const(m);
	define_class(m);
	define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	static char *buf = NULL;
	
	if (buf == NULL) {
		SDL_version linked;
		SDL_GetVersion(&linked);
		buf = malloc(256);
		sprintf(buf, "Build at\t" __DATE__ "\nSimple DirectMedia Layer\t%d.%d.%d\n", linked.major, linked.minor, linked.patch);
	}
	return buf;
}
