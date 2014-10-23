/*
 * form.c
 *
 *  Created on: 2012/01/14
 *      Author: frog
 */

#define NOT_DEFINE_BOOL
#include "gui_compat.h"
#include "m_gui.h"


void connect_widget_events(WndHandle window)
{
}
// v : FormのサブクラスでValue_new_ref済みの値を渡す
void create_form_window(Value *v, WndHandle parent, int *size)
{
}
void window_destroy(WndHandle window)
{
}
void window_set_drop_handler(WndHandle window)
{
}

double window_get_opacity(Ref *r)
{
    return 0.0;
}
void window_set_opacity(Ref *r, double opacity)
{
}
void window_get_title(Value *vret, WndHandle window)
{
}
void window_set_title(WndHandle window, Str s)
{
}
void window_move(WndHandle window, int x, int y)
{
}
void window_move_center(WndHandle window)
{
}
void window_set_client_size(WndHandle window, int width, int height)
{
}
void window_set_keep_above(WndHandle window, int above)
{
}

int window_get_visible(WndHandle window)
{
    return 0;
}
void window_set_visible(WndHandle window, int show)
{
}
int window_set_icon(WndHandle window, Ref *r, RefImage *icon)
{
    return TRUE;
}
int window_message_loop(int *loop)
{
    *loop = TRUE;
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void create_image_pane_window(Value *v, WndHandle parent)
{
}
int image_pane_set_image_sub(WndHandle window, RefImage *img)
{
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

int native_timer_new(int millisec, Ref *r)
{
    return 0;
}
void native_timer_remove(Ref *r)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

int native_filemonitor_new(Ref *r, const char *path)
{
    return TRUE;
}
void native_filemonitor_remove(Ref *r)
{
}

void native_object_remove_all()
{
}
