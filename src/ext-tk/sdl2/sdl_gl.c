#include "m_sdl2.h"



static int make_sure_current_context(RefGLContext *context)
{
    if (context->gl == NULL) {
        fs->throw_errorf(mod_sdl, "SDL_GLError", "GLContext already closed");
        return FALSE;
    }

    if (gl_cur_context != context) {
        // TODO
    }

    return TRUE;
}


static int context_close(Value *vret, Value *v, RefNode *node)
{
    RefGLContext *context = Value_vp(*v);

    if (context->gl != NULL) {
        SDL_GL_DeleteContext(context->gl);
        context->gl = NULL;
    }
    if (gl_cur_context == context) {
        gl_cur_context = NULL;
    }

    return TRUE;
}
static int context_get_swap_interval(Value *vret, Value *v, RefNode *node)
{
    RefGLContext *context = Value_vp(*v);

    if (!make_sure_current_context(context)) {
        return FALSE;
    }
    *vret = int32_Value(SDL_GL_GetSwapInterval());

    return TRUE;
}
static int context_set_swap_interval(Value *vret, Value *v, RefNode *node)
{
    RefGLContext *context = Value_vp(*v);
    int64_t interval = fs->Value_int64(v[1], NULL);

    if (!make_sure_current_context(context)) {
        return FALSE;
    }
    SDL_GL_SetSwapInterval(interval > 0LL ? 1 : interval < 0LL ? -1 : 0);

    return TRUE;
}


void define_gl_func(RefNode *m)
{
//    RefNode *n;
}
void define_gl_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls = cls_glcontext;

    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, context_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, context_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "swap_interval", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, context_get_swap_interval, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "swap_interval=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, context_set_swap_interval, 1, 1, NULL, fs->cls_int);

    fs->extends_method(cls, fs->cls_obj);
}
