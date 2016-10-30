#include "gui_compat.h"
#include "gui.h"
#include <stdlib.h>


static const char * const WINDOW_DATA_KEY = "data";

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
    }
    return result;
}

void native_timer_new(int millisec, Ref *r)
{
    int id = g_timeout_add(millisec, timeout_callback, r);

    r->v[INDEX_TIMER_ID] = int32_Value(id);
    r->rh.nref++;
}
void native_timer_remove(Ref *r)
{
    if (r->v[INDEX_TIMER_ID] != VALUE_NULL) {
        int timer_id = Value_integral(r->v[INDEX_TIMER_ID]);

        g_source_remove(timer_id);
        r->v[INDEX_TIMER_ID] = VALUE_NULL;
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

    switch (event_type) {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        action_val = "changed";
        break;
    case G_FILE_MONITOR_EVENT_MOVED:
        // deleted + created
        action_val = "deleted";
        break;
    case G_FILE_MONITOR_EVENT_DELETED:
        action_val = "deleted";
        break;
    case G_FILE_MONITOR_EVENT_CREATED:
        action_val = "created";
        break;
    default:
        return;
    }

    fs->Value_push("v", sender_r->v[INDEX_FILEMONITOR_FN]);
    evt = fg->stk_top++;
    *evt = event_object_new(sender_r);

    v_tmp = fs->cstr_Value(fs->cls_str, action_val, -1);
    event_object_add(*evt, "action", v_tmp);
    fs->unref(v_tmp);

    if (file != NULL) {
        v_tmp = gfile_to_value(file);
        event_object_add(*evt, "file", v_tmp);
        fs->unref(v_tmp);
    }
    ret_code = fs->call_function_obj(1);
    if (!ret_code && fg->error != VALUE_NULL) {
        //
    }
    fs->Value_pop();

    if (other_file != NULL && event_type == G_FILE_MONITOR_EVENT_MOVED) {
        action_val = "crated";

        fs->Value_push("v", sender_r->v[INDEX_FILEMONITOR_FN]);
        evt = fg->stk_top++;
        *evt = event_object_new(sender_r);

        v_tmp = fs->cstr_Value(fs->cls_str, action_val, -1);
        event_object_add(*evt, "action", v_tmp);
        fs->unref(v_tmp);

        v_tmp = gfile_to_value(other_file);
        event_object_add(*evt, "file", v_tmp);
        fs->unref(v_tmp);

        ret_code = fs->call_function_obj(1);
        if (!ret_code && fg->error != VALUE_NULL) {
            //
        }
        fs->Value_pop();
    }
}
int native_dirmonitor_new(Ref *r, const char *path)
{
    FileMonitor *fm = malloc(sizeof(FileMonitor));
    GFileType type;
    GError *err = NULL;

    fm->file = g_file_new_for_path(path);
    type = g_file_query_file_type(fm->file, G_FILE_QUERY_INFO_NONE, NULL);

    if (type == G_FILE_TYPE_DIRECTORY) {
        fm->monitor = g_file_monitor_directory(fm->file, G_FILE_MONITOR_NONE, NULL, &err);
    } else {
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
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
void native_dirmonitor_remove(Ref *r)
{
    FileMonitor *fm = Value_ptr(r->v[INDEX_FILEMONITOR_STRUCT]);

    if (fm != NULL) {
        g_object_unref(fm->monitor);
        g_object_unref(fm->file);
    }

    free(fm);
    r->v[INDEX_FILEMONITOR_STRUCT] = VALUE_NULL;
}
