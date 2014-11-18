#include "gui_compat.h"
#include "gui.h"
#include <stdlib.h>


static const char * const WINDOW_DATA_KEY = "data";

static gboolean signal_close(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    Ref *sender_r = g_object_get_data(G_OBJECT(widget), WINDOW_DATA_KEY);
    Value *eh = get_event_handler(sender_r, fs->intern("closed", -1));
    int result = TRUE;

    if (eh != NULL) {
        Value evt = event_object_new(sender_r);
        if (!invoke_event_handler(&result, *eh, evt)) {
            //
        }
        fs->Value_dec(evt);
    }
    return !result;
}
static void signal_destroy(GObject *object, gpointer user_data)
{
    Ref *r = g_object_get_data(object, WINDOW_DATA_KEY);

    widget_handler_destroy(r);
    fs->Value_dec(r->v[INDEX_WIDGET_HANDLE]);
    r->v[INDEX_WIDGET_HANDLE] = VALUE_NULL;
    root_window_count--;
}
static void signal_drag_data_received(
        GtkWidget *widget, GdkDragContext *context,
        gint x, gint y,
        GtkSelectionData *selection_data,
        guint info, guint tm)
{
    Ref *sender_r = g_object_get_data(G_OBJECT(widget), WINDOW_DATA_KEY);
    Value *eh = get_event_handler(sender_r, fs->intern("dropped", -1));

    if (eh != NULL) {
        int i;
        gchar **files = g_strsplit((const gchar*)gtk_selection_data_get_data(selection_data), "\r\n", 0);
        Value evt;
        int result;
        RefArray *afile = fs->refarray_new(0);

        for (i = 0; files[i] != NULL && *files[i] != '\0'; i++) {
            char *filename = g_filename_from_uri(files[i], NULL, NULL);
            if (filename != NULL) {
                Value *vf = fs->refarray_push(afile);
                *vf = fs->cstr_Value(fs->cls_file, filename, -1);
                g_free(filename);
            }
        }
        g_strfreev(files);

        evt = event_object_new(sender_r);
        event_object_add(evt, "files", vp_Value(afile));
        fs->Value_dec(vp_Value(afile));

        event_object_add(evt, "x", int32_Value(x));
        event_object_add(evt, "y", int32_Value(y));
        if (!invoke_event_handler(&result, *eh, evt)) {
            //
        }

        fs->Value_dec(evt);
    }
}

static int button_proc_sub(GtkWidget *widget, const char *type, int button, int x, int y, int dir)
{
    Ref *sender_r = g_object_get_data(G_OBJECT(widget), WINDOW_DATA_KEY);
    Value *eh = get_event_handler(sender_r, fs->intern(type, -1));

    if (eh != NULL) {
        Value v_tmp;
        const char *button_val = NULL;
        Value evt = event_object_new(sender_r);

        switch (button) {
        case 1:
            button_val = "left";
            break;
        case 2:
            button_val = "middle";
            break;
        case 3:
            button_val = "right";
            break;
        }
        if (button_val != NULL) {
            v_tmp = fs->cstr_Value(fs->cls_str, button_val, -1);
            event_object_add(evt, "button", v_tmp);
            fs->Value_dec(v_tmp);
        }
        event_object_add(evt, "x", int32_Value(x));
        event_object_add(evt, "y", int32_Value(y));

        if (button == -1) {
            event_object_add(evt, "dir", int32_Value(dir));
        }

        if (!invoke_event_handler(NULL, *eh, evt)) {
            //
        }

        fs->Value_dec(evt);
    }
    return TRUE;
}
static gboolean signal_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    button_proc_sub(widget, "button_pressed", event->button, (int)event->x, (int)event->y, 0);
    return FALSE;
}
static gboolean signal_button_released(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    button_proc_sub(widget, "button_released", event->button, (int)event->x, (int)event->y, 0);
    return FALSE;
}
static gboolean signal_mousewheel(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
    int dir;

    switch (event->direction) {
    case GDK_SCROLL_UP:
        dir = -1;
        break;
    case GDK_SCROLL_DOWN:
        dir = 1;
        break;
    default:
        dir = 0;
        break;
    }

    button_proc_sub(widget, "wheel_scrolled", -1, (int)event->x, (int)event->y, dir);
    return FALSE;
}

/**
 * Widget共通のイベントハンドラを登録する
 */
void connect_widget_events(WndHandle window)
{
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
    g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(signal_button_pressed), NULL);
    g_signal_connect(G_OBJECT(window), "button-release-event", G_CALLBACK(signal_button_released), NULL);
    g_signal_connect(G_OBJECT(window), "scroll-event", G_CALLBACK(signal_mousewheel), NULL);
}

// v : FormのサブクラスでValue_new_ref済みの値を渡す
void create_form_window(Value *v, WndHandle parent, int *size)
{
    Ref *r = Value_ref(*v);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    // Windowに関連付けたため、+1する
    fs->Value_inc(*v);
    root_window_count++;
    r->v[INDEX_WIDGET_HANDLE] = handle_Value(window);
    g_object_set_data(G_OBJECT(window), WINDOW_DATA_KEY, r);

    if (parent != NULL) {
        gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(parent));
    }
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(signal_close), NULL);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(signal_destroy), NULL);
    connect_widget_events(window);

    if (size != NULL) {
        gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
        gtk_widget_set_size_request(GTK_WIDGET(window), size[0], size[1]);
    }
}
void window_destroy(WndHandle window)
{
    gtk_widget_destroy(window);
}

void window_set_drop_handler(WndHandle window)
{
    GtkTargetEntry drag_types[] = {{(char*)"text/uri-list", 0, 0}};
    gtk_drag_dest_set(
            window,
            GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
            drag_types, 1,
            GDK_ACTION_COPY);
    g_signal_connect(G_OBJECT(window), "drag-data-received", G_CALLBACK(signal_drag_data_received), NULL);
}

double window_get_opacity(Ref *r)
{
    WndHandle window = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
    return gtk_widget_get_opacity(GTK_WIDGET(window));
}
void window_set_opacity(Ref *r, double opacity)
{
    WndHandle window = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
    gtk_widget_set_opacity(GTK_WIDGET(window), opacity);
}
void window_get_title(Value *vret, WndHandle window)
{
    const char *title = gtk_window_get_title(GTK_WINDOW(window));
    *vret = fs->cstr_Value_conv(title, -1, NULL);
}
void window_set_title(WndHandle window, const char *s)
{
    gtk_window_set_title(GTK_WINDOW(window), s);
}
void window_move(WndHandle window, int x, int y)
{
    if (x < 0) {
        x += gdk_screen_width();
    }
    if (y < 0) {
        y += gdk_screen_height();
    }
    gtk_window_move(GTK_WINDOW(window), x, y);
}
// TODO
void window_move_center(WndHandle window)
{
}
void window_set_client_size(WndHandle window, int width, int height)
{
    // クライアント領域のサイズを設定
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
}

int window_get_visible(WndHandle window)
{
    return gtk_widget_get_visible(window);
}
void window_set_visible(WndHandle window, int show)
{
    if (show) {
        gtk_widget_show_all(window);
    } else {
        gtk_widget_hide(window);
    }
}
void window_set_keep_above(WndHandle window, int above)
{
    gtk_window_set_keep_above(GTK_WINDOW(window), above);
}
int window_set_icon(WndHandle window, Ref *r, RefImage *icon)
{
    GraphicsHandle img = conv_image_to_graphics(icon);
    if (img == NULL) {
        return FALSE;
    }
    gtk_window_set_icon(GTK_WINDOW(window), img);
    return TRUE;
}
int window_message_loop(int *loop)
{
    if (gtk_main_iteration_do(TRUE)) {
    } else {
    }
    *loop = TRUE;
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static gboolean signal_mousewheel_void(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
    return TRUE;
}
void create_image_pane_window(Value *v, WndHandle parent)
{
    Ref *r = Value_ref(*v);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *window = gtk_image_new();

    g_object_set_data(G_OBJECT(window), WINDOW_DATA_KEY, r);
    gtk_container_add(GTK_CONTAINER(scroll), window);

    if (parent != NULL) {
        gtk_container_add(GTK_CONTAINER(parent), scroll);
    }
    connect_widget_events(window);

    // ホイールスクロールを抑止
    gtk_widget_add_events(scroll, GDK_SCROLL_MASK);
    g_signal_connect(G_OBJECT(scroll), "scroll-event", G_CALLBACK(signal_mousewheel_void), NULL);

    r->v[INDEX_WIDGET_HANDLE] = handle_Value(window);

    fs->Value_inc(*v);
}
int image_pane_set_image_sub(WndHandle window, RefImage *img)
{
    GraphicsHandle handle = conv_image_to_graphics(img);

    if (handle == NULL) {
        return FALSE;
    }
    gtk_image_set_from_pixbuf(GTK_IMAGE(window), handle);
    g_object_unref(handle);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static gint timeout_callback(gpointer data)
{
    Ref *sender_r = (Ref*)data;
    Value *stk_top = fg->stk_top;
    int ret_code;
    int result = TRUE;

    fs->Value_push("v", sender_r->v[INDEX_TIMER_FN]);
    *fg->stk_top++ = event_object_new(sender_r);
    ret_code = fs->call_function_obj(1);

    if (ret_code) {
        Value vr = fg->stk_top[-1];
        if (vr == VALUE_FALSE) {
            result = FALSE;
        }
    } else {
        result = FALSE;
    }

    // エラーの場合、stk_topが不定
    while (fg->stk_top > stk_top) {
        fs->Value_pop();
    }
    if (!result) {
        sender_r->v[INDEX_TIMER_ID] = VALUE_NULL;

        // カウンタを1減らす
        fs->Value_dec(vp_Value(sender_r));
        root_window_count--;
    }
    return result;
}

void native_timer_new(int millisec, Ref *r)
{
    int id = g_timeout_add(millisec, timeout_callback, r);

    r->v[INDEX_TIMER_ID] = int32_Value(id);
    r->rh.nref++;
    // イベントループ監視対象なので1増やす
    root_window_count++;
}
void native_timer_remove(Ref *r)
{
    if (r->v[INDEX_TIMER_ID] != VALUE_NULL) {
        int timer_id = Value_integral(r->v[INDEX_TIMER_ID]);

        g_source_remove(timer_id);
        r->v[INDEX_TIMER_ID] = VALUE_NULL;

        // カウンタを1減らす
        fs->Value_dec(vp_Value(r));
        root_window_count--;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    GFile *file;
    GFileMonitor *monitor;
} FileMonitor;

static Value gfile_to_value(GFile *file)
{
    if (file != NULL) {
        char *filename = g_file_get_path(file);
        if (filename != NULL) {
            Value v = fs->cstr_Value(fs->cls_file, filename, -1);
            g_free(filename);
            return v;
        } else {
            return VALUE_NULL;
        }
    } else {
        return VALUE_NULL;
    }
}

void signal_file_changed(
    GFileMonitor *monitor, GFile *file, GFile *other_file,
    GFileMonitorEvent event_type, gpointer user_data)
{
    Ref *sender_r = g_object_get_data(G_OBJECT(monitor), WINDOW_DATA_KEY);
    Value *evt;
    Value v_tmp;
    int ret_code;
    const char *action_val = NULL;

    fs->Value_push("v", &sender_r->v[INDEX_FILEMONITOR_FN]);
    evt = fg->stk_top++;
    *evt = event_object_new(sender_r);

    switch (event_type) {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        action_val = "changed";
        break;
    case G_FILE_MONITOR_EVENT_MOVED:
        action_val = "moved";
        break;
    case G_FILE_MONITOR_EVENT_DELETED:
        action_val = "deleted";
        break;
    case G_FILE_MONITOR_EVENT_CREATED:
        action_val = "created";
        break;
    default:
        action_val = "others";
        break;
    }
    v_tmp = fs->cstr_Value(fs->cls_str, action_val, -1);
    event_object_add(*evt, "action", v_tmp);
    fs->Value_dec(v_tmp);

    v_tmp = gfile_to_value(file);
    event_object_add(*evt, "file", v_tmp);
    fs->Value_dec(v_tmp);

    v_tmp = gfile_to_value(other_file);
    event_object_add(*evt, "other_file", v_tmp);
    fs->Value_dec(v_tmp);

    ret_code = fs->call_function_obj(1);
    if (!ret_code && fg->error != VALUE_NULL) {
        //
    }
    fs->Value_pop();
}
int native_filemonitor_new(Ref *r, const char *path)
{
    FileMonitor *fm = malloc(sizeof(FileMonitor));
    GFileType type;
    GError *err = NULL;

    fm->file = g_file_new_for_path(path);
    type = g_file_query_file_type(fm->file, G_FILE_QUERY_INFO_NONE, NULL);

    if (type == G_FILE_TYPE_DIRECTORY) {
        fm->monitor = g_file_monitor_directory(fm->file, G_FILE_MONITOR_NONE, NULL, &err);
    } else if (type == G_FILE_TYPE_REGULAR) {
        fm->monitor = g_file_monitor_file(fm->file, G_FILE_MONITOR_NONE, NULL, &err);
    } else {
        return FALSE;
    }
    if (fm->monitor == NULL) {
        fs->throw_errorf(fs->mod_io, "IOError", "%s", err != NULL ? err->message : "An error occured");
        g_object_unref(fm->file);
        free(fm);
        return FALSE;
    }

    g_object_set_data(G_OBJECT(fm->monitor), WINDOW_DATA_KEY, r);
    g_signal_connect(G_OBJECT(fm->monitor), "changed", G_CALLBACK(signal_file_changed), NULL);
    r->v[INDEX_FILEMONITOR_STRUCT] = ptr_Value(fm);

    return TRUE;
}
void native_filemonitor_remove(Ref *r)
{
    FileMonitor *fm = Value_ptr(r->v[INDEX_FILEMONITOR_STRUCT]);

    if (fm != NULL) {
        g_object_unref(fm->monitor);
        g_object_unref(fm->file);
        free(fm);
        r->v[INDEX_FILEMONITOR_STRUCT] = VALUE_NULL;
    }
}

void native_object_remove_all()
{
}
