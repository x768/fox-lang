#define DEFINE_GLOBALS
#include "gui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



static WndHandle Value_widget_handle(Value v)
{
    Ref *r = Value_ref(v);
    return Value_handle(r->v[INDEX_WIDGET_HANDLE]);
}
static int check_already_closed(void *handle)
{
    if (handle == NULL) {
        fs->throw_errorf(mod_gui, "GuiError", "Widget already disposed");
        return FALSE;
    }
    return TRUE;
}

static int validate_string_array(RefArray *ra)
{
    int i;
    for (i = 0; i < ra->size; i++) {
        RefNode *type = fs->Value_type(ra->p[i]);
        if (type != fs->cls_str) {
            return FALSE;
        }
    }
    return TRUE;
}

Value *get_event_handler(Ref *r, RefStr *name)
{
    RefGuiHash *hash = Value_vp(r->v[INDEX_WBASE_HANDLER]);

    if (hash != NULL) {
        Value *val = GuiHash_get_p(hash, name);
        if (val != NULL) {
            RefArray *ra = Value_vp(*val);
            if (ra->size > 0) {
                return val;
            }
        }
    }
    return NULL;
}
int invoke_event_handler(int *result, Value eh, Value evt)
{
    fs->Value_push("vv", eh, evt);
    if (!fs->call_member_func(fs->symbol_stock[T_LP], 1, TRUE)) {
        return FALSE;
    }
    if (result != NULL) {
        *result = Value_bool(fg->stk_top[-1]);
    }
    fs->Value_pop();

    return TRUE;
}

void widget_event_handler_sub(Value *vret, Ref *r, RefStr *name)
{
    RefGuiHash *hash = Value_vp(r->v[INDEX_WBASE_HANDLER]);
    Value *val;

    if (hash == NULL) {
        hash = malloc(sizeof(RefGuiHash));
        memset(hash, 0, sizeof(RefGuiHash));
        r->v[INDEX_WBASE_HANDLER] = vp_Value(hash);
    }
    val = GuiHash_add_p(hash, name);

    if (*val == VALUE_NULL) {
        RefArray *ra = fs->refarray_new(0);
        *val = vp_Value(ra);
        ra->rh.type = cls_evt_handler;
    }
    *vret = fs->Value_cp(*val);
}
int widget_handler_destroy(Ref *r)
{
    RefGuiHash *hash = Value_vp(r->v[INDEX_WBASE_HANDLER]);

    if (hash != NULL) {
        GuiHash_close(hash);
        free(hash);
        r->v[INDEX_WBASE_HANDLER] = VALUE_NULL;
    }
    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////

#define PTR_TO_HASH(p) ((((intptr_t)(p)) >> 16) & (GUI_ENTRY_NUM - 1))

void GuiHash_init(RefGuiHash *h)
{
    memset(h->entry, 0, sizeof(h->entry));
}
Value *GuiHash_get_p(RefGuiHash *h, const void *key)
{
    GuiEntry *ge = h->entry[PTR_TO_HASH(key)];
    while (ge != NULL) {
        if (ge->key == key) {
            return &ge->val;
        }
        ge = ge->next;
    }
    return NULL;
}
Value *GuiHash_add_p(RefGuiHash *h, const void *key)
{
    GuiEntry **ppg = &h->entry[PTR_TO_HASH(key)];
    GuiEntry *pg = *ppg;
    GuiEntry *ge;

    while (pg != NULL) {
        if (pg->key == key) {
            return &pg->val;
        }
        ppg = &pg->next;
        pg = *ppg;
    }

    ge = malloc(sizeof(GuiEntry));
    ge->key = key;
    ge->next = NULL;
    ge->val = VALUE_NULL;
    *ppg = ge;

    return &ge->val;
}
void GuiHash_del_p(RefGuiHash *h, const void *key)
{
    GuiEntry **ppg = &h->entry[PTR_TO_HASH(key)];

    while (*ppg != NULL) {
        GuiEntry *pg = *ppg;
        if (pg->key == key) {
            *ppg = pg->next;
            fs->unref(pg->val);
            free(pg);
            break;
        }
        ppg = &pg->next;
    }
}
void GuiHash_close(RefGuiHash *h)
{
    int i;
    for (i = 0; i < GUI_ENTRY_NUM; i++) {
        GuiEntry *ge = h->entry[i];
        while (ge != NULL) {
            GuiEntry *prev = ge;
            ge = ge->next;
            fs->unref(prev->val);
            free(prev);
        }
    }
}

Value event_object_new(Ref *sender_r)
{
    Value sender;
    Value evt;
    RefGuiHash *h = fs->buf_new(cls_event, sizeof(RefGuiHash));
    GuiHash_init(h);

    sender = fs->Value_cp(vp_Value(sender_r));
    evt = vp_Value(h);
    event_object_add(evt, "sender", sender);
    return evt;
}
void event_object_add(Value evt, const char *name, Value val)
{
    RefGuiHash *h = Value_vp(evt);
    RefStr *k = fs->intern(name, -1);
    Value *v = GuiHash_add_p(h, k);

    fs->unref(*v);
    *v = fs->Value_cp(val);
}

//////////////////////////////////////////////////////////////////////////////////////

static int event_close(Value *vret, Value *v, RefNode *node)
{
    RefGuiHash *h = Value_vp(*v);
    GuiHash_close(h);
    return TRUE;
}
static int event_get(Value *vret, Value *v, RefNode *node)
{
    RefGuiHash *h = Value_vp(*v);
    RefStr *rs = Value_vp(v[1]);
    Value *val = GuiHash_get_p(h, fs->intern(rs->c, rs->size));

    if (val == NULL) {
        fs->throw_errorf(fs->mod_lang, "NameError", "Event has no member named %r", rs);
        return FALSE;
    }
    *vret = fs->Value_cp(*val);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////

static int gui_alert(Value *vret, Value *v, RefNode *node)
{
    RefStr *msg;
    RefStr *title;
    WndHandle parent = NULL;
    RefNode *type = fs->Value_type(v[1]);
    int confirm = FUNC_INT(node);

    if (fg->stk_top > v + 2) {
        title = Value_vp(v[2]);
    } else {
        title = NULL;
    }
    if (fg->stk_top > v + 3) {
        parent = Value_widget_handle(v[3]);
    }

    if (type == fs->cls_str) {
        msg = Value_vp(v[1]);
    } else {
        fs->Value_push("v", v[1]);
        if (!fs->call_member_func(fs->str_tostr, 0, TRUE)) {
            return FALSE;
        }

        type = fs->Value_type(fg->stk_top[-1]);
        if (type != fs->cls_str) {
            fs->throw_error_select(THROW_RETURN_TYPE__NODE_NODE, fs->cls_str, type);
            return FALSE;
        }
        msg = Value_vp(fg->stk_top[-1]);
    }

    if (confirm) {
        int ret = confirm_dialog(msg, title, parent);
        *vret = bool_Value(ret);
    } else {
        alert_dialog(msg, title, parent);
    }

    return TRUE;
}
static int gui_fileopen_dialog(Value *vret, Value *v, RefNode *node)
{
    RefStr *title = Value_vp(v[1]);
    RefArray *ra = NULL;
    WndHandle parent = NULL;
    int type = FUNC_INT(node);

    if (type == FILEOPEN_DIR) {
        if (fg->stk_top > v + 2) {
            parent = Value_widget_handle(v[2]);
        }
    } else {
        if (fg->stk_top > v + 2) {
            ra = Value_vp(v[2]);
            if (!validate_string_array(ra)) {
                fs->throw_errorf(fs->mod_lang, "TypeError", "Array of Str required (argument #2)");
                return FALSE;
            }
        }
        if (fg->stk_top > v + 3) {
            parent = Value_widget_handle(v[3]);
        }
    }

    file_open_dialog(vret, title->c, ra, parent, FUNC_INT(node));
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int clear_clip(Value *vret, Value *v, RefNode *node)
{
    clipboard_clear();
    return TRUE;
}
static int is_clip_available(Value *vret, Value *v, RefNode *node)
{
    RefNode *klass = Value_vp(v[1]);
    *vret = bool_Value(clipboard_is_available(klass));
    return TRUE;
}
static int get_clip_data(Value *vret, Value *v, RefNode *node)
{
    RefNode *klass = Value_vp(v[1]);

    if (klass == fs->cls_str) {
        *vret = clipboard_get_text();
    } else if (klass == fs->cls_file) {
        clipboard_get_files(vret, FALSE);
    } else if (klass == fs->cls_uri) {
        clipboard_get_files(vret, TRUE);
    } else if (klass == cls_image) {
        clipboard_get_image(vret);
    }
    return TRUE;
}
static int set_clip_data(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefNode *v1_type = fs->Value_type(v1);

    if (v1_type == fs->cls_str) {
        RefStr *rs = Value_vp(v1);
        clipboard_set_text(rs->c, rs->size);
    } else if (v1_type == cls_image) {
        RefImage *img = Value_vp(v1);
        if (img->data == NULL) {
            fs->throw_errorf(mod_image, "ImageError", "Image is already closed");
            return FALSE;
        }
        clipboard_set_image(img);
    } else if (v1_type == fs->cls_list) {
        RefArray *ra = Value_vp(v1);
        clipboard_set_files(ra->p, ra->size);
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Str, Image or Array required but %n (argument #1)", v1_type);
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int uri_to_file(Value *vret, Value *v, RefNode *node)
{
    if (!uri_to_file_sub(vret, v[1])) {
        return FALSE;
    }
    return TRUE;
}
static int exio_trash(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = fs->Value_type(v[1]);
    if (type == fs->cls_uri || type == fs->cls_str || type == fs->cls_file) {
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Str, File or Uri required but %n(argument #1)", type);
        return FALSE;
    }

    if (!exio_trash_sub(v[1])) {
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int widget_is_destroyed(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    *vret = bool_Value(window == NULL);
    return TRUE;
}
static int widget_get_visible(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    *vret = bool_Value(window_get_visible(window));
    return TRUE;
}
static int widget_set_visible(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    window_set_visible(window, Value_bool(v[1]));

    return TRUE;
}

static int widget_handler(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefStr *name;

    if (fg->stk_top > v + 1) {
        RefStr *rs = Value_vp(v[1]);
        name = fs->intern(rs->c, rs->size);
    } else {
        name = node->name;
    }
    widget_event_handler_sub(vret, r, name);

    return TRUE;
}
static int widget_call_handler(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////

static int value_to_winhandle(WndHandle *pwh, Value v, int argc, RefNode *a_type)
{
    RefNode *type = fs->Value_type(v);

    if (fs->is_subclass(type, a_type)) {
        Ref *r = Value_ref(v);
        *pwh = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
    } else if (type == fs->cls_null) {
        *pwh = NULL;
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, a_type, fs->cls_null, type, argc);
        return FALSE;
    }
    return TRUE;
}

static int form_new_sub(Value *vret, Value *v, WndHandle parent, int *size)
{
    RefNode *type = fs->Value_type(*v);
    Ref *r;

    // 継承可能なクラス
    if (type == fs->cls_fn) {
        r = fs->ref_new(cls_form);
        *vret = vp_Value(r);
    } else {
        r = Value_vp(*v);
        *vret = fs->Value_cp(*v);
    }
    create_form_window(r, parent, size);

    return TRUE;
}
static int form_new(Value *vret, Value *v, RefNode *node)
{
    WndHandle parent = NULL;

    if (fg->stk_top > v + 1) {
        if (!value_to_winhandle(&parent, v[1], 1, cls_form)) {
            return FALSE;
        }
    }
    return form_new_sub(vret, v, parent, NULL);
}
static int form_fixed(Value *vret, Value *v, RefNode *node)
{
    WndHandle parent = NULL;
    int size[2];

    size[0] = fs->Value_int64(v[1], NULL);
    size[1] = fs->Value_int64(v[2], NULL);

    if (fg->stk_top > v + 3) {
        if (!value_to_winhandle(&parent, v[3], 3, cls_form)) {
            return FALSE;
        }
    }
    return form_new_sub(vret, v, parent, size);
}
static int form_close(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    Ref *r = Value_ref(*v);
    if (window != NULL) {
        window_destroy(window);
    }
    widget_handler_destroy(r);
    return TRUE;
}

static int form_get_title(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    if (!check_already_closed(window)) {
        return FALSE;
    }
    window_get_title(vret, window);

    return TRUE;
}
static int form_set_title(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    RefStr *rs = Value_vp(v[1]);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    window_set_title(window, rs->c);

    return TRUE;
}
static int form_set_icon(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    RefImage *img = Value_vp(v[1]);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    if (img->data == NULL) {
        fs->throw_errorf(mod_image, "ImageError", "Image is already closed");
        return FALSE;
    }

    if (!window_set_icon(window, Value_vp(*v), img)) {
        return FALSE;
    }

    return TRUE;
}
static int form_get_opacity(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    WndHandle window = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
    if (!check_already_closed(window)) {
        return FALSE;
    }
    *vret = fs->float_Value(fs->cls_float, window_get_opacity(r));

    return TRUE;
}
static int form_set_opacity(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    WndHandle window = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
    double opacity = fs->Value_float(v[1]);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    if (opacity < 0.0) {
        opacity = 0.0;
    } else if (opacity > 1.0) {
        opacity = 1.0;
    }
    window_set_opacity(r, opacity);

    return TRUE;
}
static int form_set_client_size(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    int width = fs->Value_int64(v[1], NULL);
    int height = fs->Value_int64(v[2], NULL);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    window_set_client_size(window, width, height);

    return TRUE;
}
static int form_move(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    int x = fs->Value_int64(v[1], NULL);
    int y = fs->Value_int64(v[2], NULL);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    window_move(window, x, y);

    return TRUE;
}
static int form_move_center(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    window_move_center(window);

    return TRUE;
}

static int form_keep_above(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    int above = Value_bool(v[1]);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    window_set_keep_above(window, above);

    return TRUE;
}
static int form_add_menu(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    WndHandle window = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
    Ref *r2 = Value_ref(v[2]);
    MenuHandle menu = Value_handle(r2->v[INDEX_MENU_HANDLE]);
    MenuHandle menubar = NULL;
    RefStr *text = Value_vp(v[1]);
    int init = FALSE;

    if (!check_already_closed(window)) {
        return FALSE;
    }

    menubar = Value_handle(r->v[INDEX_FORM_MENU]);
    if (menubar == NULL) {
        menubar = Menubar_new();
        r->v[INDEX_FORM_MENU] = handle_Value(menubar);
        init = TRUE;
    }
    Menu_add_submenu(menubar, menu, text->c);
    if (init) {
        Window_set_menu(window, menubar);
    }

    return TRUE;
}
static int form_wait(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    if (!check_already_closed(Value_handle(r->v[INDEX_WIDGET_HANDLE]))) {
        return FALSE;
    }

    for (;;) {
        if (Value_handle(r->v[INDEX_WIDGET_HANDLE]) == NULL) {
            break;
        }
        if (!window_message_loop()) {
            return FALSE;
        }
        if (fg->error != VALUE_NULL) {
            return FALSE;
        }
    }

    return TRUE;
}
static int form_handler_dropped(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    WndHandle window = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
    RefStr *name;

    if (!check_already_closed(window)) {
        return FALSE;
    }
    if (fg->stk_top > v + 1) {
        RefStr *rs = Value_vp(v[1]);
        name = fs->intern(rs->c, rs->size);
    } else {
        name = node->name;
    }
    widget_event_handler_sub(vret, r, name);

    // 最初の1回だけ登録
    if (!Value_bool(r->v[INDEX_FORM_DROP_HANDLE])) {
        window_set_drop_handler(window);
        r->v[INDEX_FORM_DROP_HANDLE] = VALUE_TRUE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int image_pane_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_image_pane = FUNC_VP(node);
    WndHandle parent = Value_widget_handle(v[1]);

    *vret = vp_Value(fs->ref_new(cls_image_pane));
    create_image_pane_window(vret, parent);
    return TRUE;
}
static int image_pane_set_image(Value *vret, Value *v, RefNode *node)
{
    WndHandle window = Value_widget_handle(*v);
    RefImage *img = Value_vp(v[1]);

    if (!check_already_closed(window)) {
        return FALSE;
    }
    if (!image_pane_set_image_sub(window, img)) {
        return FALSE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int timer_new(Value *vret, Value *v, RefNode *node)
{
    int millisec = fs->Value_int64(v[1], NULL);
    Ref *r = fs->ref_new(cls_timer);
    *vret = vp_Value(r);

    if (millisec <= 0) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "argument #1 must be a positive integer");
        return FALSE;
    }
    r->v[INDEX_TIMER_FN] = fs->Value_cp(v[2]);
    native_timer_new(millisec, r);

    return TRUE;
}
static int timer_wait(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    for (;;) {
        if (r->v[INDEX_TIMER_ID] == VALUE_NULL) {
            break;
        }
        if (!window_message_loop()) {
            return FALSE;
        }
        if (fg->error != VALUE_NULL) {
            return FALSE;
        }
    }
    return TRUE;
}
static int timer_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    if (r->v[INDEX_TIMER_ID] != VALUE_NULL) {
        native_timer_remove(r);
    }
    fs->unref(r->v[INDEX_TIMER_FN]);
    r->v[INDEX_TIMER_FN] = VALUE_NULL;
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int dirmonitor_new(Value *vret, Value *v, RefNode *node)
{
    Str path_s;
    Ref *r = fs->ref_new(cls_dirmonitor);
    char *path = fs->file_value_to_path(&path_s, v[1], 0);

    *vret = vp_Value(r);
    if (path == NULL) {
        return FALSE;
    }
    if (!native_dirmonitor_new(r, path)) {
        return FALSE;
    }
    r->v[INDEX_FILEMONITOR_FILE] = fs->cstr_Value(fs->cls_file, path, -1);
    free(path);

    r->v[INDEX_FILEMONITOR_FN] = fs->Value_cp(v[2]);

    return TRUE;
}
static int dirmonitor_target(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = fs->Value_cp(r->v[INDEX_FILEMONITOR_FILE]);
    return TRUE;
}
static int dirmonitor_wait(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    if (r->v[INDEX_FILEMONITOR_STRUCT] == VALUE_NULL) {
        fs->throw_errorf(mod_gui, "GuiError", "DirMonitor already disposed");
        return FALSE;
    }

    for (;;) {
        if (!window_message_loop()) {
            return FALSE;
        }
        if (r->v[INDEX_FILEMONITOR_STRUCT] == VALUE_NULL) {
            break;
        }
    }
    if (fg->error != VALUE_NULL) {
        return FALSE;
    }

    return TRUE;
}
static int dirmonitor_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    if (r->v[INDEX_FILEMONITOR_STRUCT] != VALUE_NULL) {
        native_dirmonitor_remove(r);
        r->v[INDEX_FILEMONITOR_STRUCT] = VALUE_NULL;
    }
    fs->unref(r->v[INDEX_FILEMONITOR_FN]);
    r->v[INDEX_FILEMONITOR_FN] = VALUE_NULL;
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int menu_new(Value *vret, Value *v, RefNode *node)
{
    int i;
    int argc = fg->stk_top - v;
    MenuHandle menu = Menu_new();
    Ref *r = fs->ref_new(cls_menu);
    *vret = vp_Value(r);

    for (i = 1; i < argc; ) {
        RefNode *v_type = fs->Value_type(v[i]);
        if (v_type == fs->cls_str) {
            RefStr *text = Value_vp(v[i]);
            i++;
            if (i < argc) {
                RefNode *v2_type = fs->Value_type(v[i]);
                if (v2_type == fs->cls_fn) {
                    Menu_add_item(menu, v[i], text->c);
                } else if (v2_type == cls_menu) {
                    Ref *menu_r = Value_ref(v[i]);
                    if (Value_bool(menu_r->v[INDEX_MENU_HAS_PARENT])) {
                        fs->throw_errorf(mod_gui, "GuiError", "The menu already has a parent");
                        return FALSE;
                    } else {
                        MenuHandle submenu = Value_handle(menu_r->v[INDEX_MENU_HANDLE]);
                        Menu_add_submenu(menu, submenu, text->c);
                        menu_r->v[INDEX_MENU_HAS_PARENT] = VALUE_TRUE;
                    }
                } else {
                    fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_fn, cls_menu, v_type, i);
                    return FALSE;
                }
                i++;
            } else {
                fs->throw_errorf(fs->mod_lang, "ArgumentError", "One more argument required");
                return FALSE;
            }
        } else if (v_type == fs->cls_null) {
            Menu_add_separator(menu);
            i++;
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, v_type, i);
            return FALSE;
        }
    }

    r->v[INDEX_MENU_VALID] = VALUE_TRUE;
    r->v[INDEX_MENU_HANDLE] = handle_Value(menu);

    return TRUE;
}
static int menu_close(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}
static int menu_add(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    MenuHandle menu = Value_handle(r->v[INDEX_MENU_HANDLE]);
    RefStr *text = Value_vp(v[1]);
    RefNode *v2_type = fs->Value_type(v[2]);

    if (v2_type == fs->cls_fn) {
        Menu_add_item(menu, v[2], text->c);
    } else if (v2_type == cls_menu) {
        Ref *menu_r = Value_ref(v[2]);
        if (Value_bool(menu_r->v[INDEX_MENU_HAS_PARENT])) {
            fs->throw_errorf(mod_gui, "GuiError", "The menu already has a parent");
            return FALSE;
        } else {
            MenuHandle submenu = Value_handle(menu_r->v[INDEX_MENU_HANDLE]);
            Menu_add_submenu(menu, submenu, text->c);
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_fn, cls_menu, v2_type, 2);
        return FALSE;
    }
    return TRUE;
}
static int menu_add_separator(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    MenuHandle menu = Value_handle(r->v[INDEX_MENU_HANDLE]);
    Menu_add_separator(menu);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int evthandler_new(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = fs->refarray_new(0);
    *vret = vp_Value(ra);
    ra->rh.type = cls_evt_handler;
    return TRUE;
}
static int evthandler_call(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    Value *argv = v + 1;
    int argc = fg->stk_top - argv;
    Value *stk_top = fg->stk_top;
    int result = TRUE;
    int i, j;

    // 追加したときと逆順
    for (i = ra->size - 1; i >= 0; i--) {
        fs->Value_push("v", ra->p[i]);
        for (j = 0; j < argc; j++) {
            fs->Value_push("v", argv[j]);
        }
        if (!fs->call_function_obj(argc)) {
            return FALSE;
        }

        {
            // falseを返したときだけ、resultをFALSEにする
            Value vr = fg->stk_top[-1];
            if (vr == VALUE_FALSE) {
                result = FALSE;
            }
        }
        while (fg->stk_top > stk_top) {
            fs->Value_pop();
        }
        if (!result) {
            break;
        }
    }
    *vret = bool_Value(result);

    return TRUE;
}
static int evthandler_del(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    Value fn = v[1];
    Ref *rfn = Value_ref(fn);
    int fn_r;
    RefNode *nfn;
    int i;

    if (rfn->rh.n_memb > 0) {
        fn_r = TRUE;
        nfn = Value_vp(rfn->v[0]);
    } else {
        fn_r = FALSE;
        nfn = (RefNode*)rfn;
    }

    for (i = ra->size - 1; i >= 0; i--) {
        Value f = ra->p[i];
        Ref *rf = Value_ref(f);
        RefNode *nf;

        if (fn_r) {
            if (rf->rh.n_memb == 0) {
                continue;
            }
            nf = Value_vp(rf->v[0]);
        } else {
            if (rf->rh.n_memb > 0) {
                continue;
            }
            nf = (RefNode*)rf;
        }

        if (nfn == nf) {
            fs->unref(f);
            if (i < ra->size - 1) {
                memmove(&ra->p[i], &ra->p[i + 1], sizeof(Value) * (ra->size - i - 1));
            }
            ra->size--;
        }
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int gui_wait_message(Value *vret, Value *v, RefNode *node)
{
    if (!window_message_loop()) {
        return FALSE;
    }

    return TRUE;
}
static int gui_quit(Value *vret, Value *v, RefNode *node)
{
    gui_dispose();
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static NativeFunc get_function_ptr(RefNode *node, RefStr *name)
{
    RefNode *n = fs->get_node_member(fs->cls_list, name, NULL);
    if (n == NULL || n->type != NODE_FUNC_N) {
        fs->fatal_errorf(NULL, "Initialize module 'foxtk' failed at get_function_ptr(%r)", name);
    }
    return n->u.f.u.fn;
}
static void define_func(RefNode *m)
{
    RefNode *n;


    n = fs->define_identifier(m, m, "wait_message", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_wait_message, 0, 0, NULL);

    n = fs->define_identifier(m, m, "alert", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_alert, 1, 3, (void*)FALSE, NULL, fs->cls_str, cls_form);
    n = fs->define_identifier(m, m, "confirm", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_alert, 1, 3, (void*)TRUE, NULL, fs->cls_str, cls_form);

    n = fs->define_identifier(m, m, "open_file_dialog", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_fileopen_dialog, 1, 3, (void*)FILEOPEN_OPEN, fs->cls_str, fs->cls_list, cls_form);
    n = fs->define_identifier(m, m, "open_files_dialog", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_fileopen_dialog, 1, 3, (void*)FILEOPEN_OPEN_MULTI, fs->cls_str, fs->cls_list, cls_form);
    n = fs->define_identifier(m, m, "save_file_dialog", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_fileopen_dialog, 1, 3, (void*)FILEOPEN_SAVE, fs->cls_str, fs->cls_list, cls_form);
    n = fs->define_identifier(m, m, "open_dir_dialog", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_fileopen_dialog, 1, 2, (void*)FILEOPEN_DIR, fs->cls_str, cls_form);

    n = fs->define_identifier(m, m, "clear_clip", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, clear_clip, 0, 0, NULL);
    n = fs->define_identifier(m, m, "is_clip_available", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, is_clip_available, 1, 1, NULL, fs->cls_class);
    n = fs->define_identifier(m, m, "clip_get", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_clip_data, 1, 1, NULL, fs->cls_class);
    n = fs->define_identifier(m, m, "clip_set", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, set_clip_data, 1, 1, NULL, NULL);

    n = fs->define_identifier(m, m, "uri_to_file", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, uri_to_file, 1, 1, NULL, fs->cls_uri);

    n = fs->define_identifier(m, m, "mv_trash", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, exio_trash, 1, 1, NULL, NULL);

    n = fs->define_identifier_p(m, m, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gui_quit, 0, 0, NULL);
}
static void define_list_method_0(RefNode *cls, const char *name, int type, RefNode *m)
{
    RefStr *rs = fs->intern(name, -1);
    RefNode *n = fs->define_identifier_p(m, cls, rs, NODE_FUNC_N, type);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, rs), 0, 0, NULL);
}
static void define_list_method_1(RefNode *cls, const char *name, RefNode *arg1, RefNode *m)
{
    RefStr *rs = fs->intern(name, -1);
    RefNode *n = fs->define_identifier_p(m, cls, rs, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, rs), 1, 1, NULL, arg1);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_menu = fs->define_identifier(m, m, "Menu", NODE_CLASS, 0);
    cls_menuitem = fs->define_identifier(m, m, "MenuItem", NODE_CLASS, 0);
    cls_widget = fs->define_identifier(m, m, "Widget", NODE_CLASS, NODEOPT_ABSTRACT);
    cls_layout = fs->define_identifier(m, m, "Layout", NODE_CLASS, NODEOPT_ABSTRACT);
    cls_form = fs->define_identifier(m, m, "Form", NODE_CLASS, NODEOPT_ABSTRACT);
    cls_tray_icon = fs->define_identifier(m, m, "TrayIcon", NODE_CLASS, NODEOPT_ABSTRACT);
    cls_event = fs->define_identifier(m, m, "Event", NODE_CLASS, 0);
    cls_evt_handler = fs->define_identifier(m, m, "EventHandler", NODE_CLASS, 0);
    cls_timer = fs->define_identifier(m, m, "Timer", NODE_CLASS, 0);
    cls_dirmonitor = fs->define_identifier(m, m, "DirMonitor", NODE_CLASS, 0);


    cls = cls_widget;
    n = fs->define_identifier(m, cls, "destroyed", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, widget_is_destroyed, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "_handler", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, widget_handler, 0, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "_call_handler", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, widget_call_handler, 1, 1, NULL, cls_event);
    n = fs->define_identifier(m, cls, "visible", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, widget_get_visible, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "visible=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, widget_set_visible, 1, 1, NULL, fs->cls_bool);
    n = fs->define_identifier(m, cls, "destroyed", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, widget_is_destroyed, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "button_pressed", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, widget_handler, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "button_released", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, widget_handler, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "wheel_scrolled", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, widget_handler, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_WIDGET_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_layout;
    fs->extends_method(cls, cls_widget);


    cls = cls_form;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, form_new, 0, 1, NULL, NULL);
    n = fs->define_identifier(m, cls, "fixed", NODE_NEW_N, 0);
    fs->define_native_func_a(n, form_fixed, 2, 3, NULL, fs->cls_int, fs->cls_int, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "title", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, form_get_title, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "title=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_set_title, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "icon=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_set_icon, 1, 1, NULL, NULL);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);
    n = fs->define_identifier(m, cls, "opacity", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, form_get_opacity, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "opacity=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_set_opacity, 1, 1, NULL, fs->cls_float);
    n = fs->define_identifier(m, cls, "set_size", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_set_client_size, 2, 2, NULL, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "move", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_move, 2, 2, NULL, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "move_center", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_move_center, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "keep_above=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_keep_above, 1, 1, NULL, fs->cls_bool);
    n = fs->define_identifier(m, cls, "add_menu", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_add_menu, 2, 2, NULL, fs->cls_str, cls_menu);
    n = fs->define_identifier(m, cls, "wait", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, form_wait, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "closed", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, widget_handler, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "dropped", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, form_handler_dropped, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_FORM_NUM;
    fs->extends_method(cls, cls_layout);


    cls = fs->define_identifier(m, m, "ImagePane", NODE_CLASS, 0);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, image_pane_new, 0, 1, cls, cls_layout);

    n = fs->define_identifier(m, cls, "image=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, image_pane_set_image, 1, 1, NULL, NULL);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);
    cls->u.c.n_memb = INDEX_FORM_NUM;
    fs->extends_method(cls, cls_widget);


    cls = cls_event;
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, event_close, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, event_get, 1, 1, NULL, NULL);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_timer;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, timer_new, 2, 2, NULL, fs->cls_int, fs->cls_fn);
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, timer_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, timer_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "wait", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, timer_wait, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_TIMER_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_dirmonitor;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, dirmonitor_new, 2, 2, NULL, NULL, fs->cls_fn);
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, dirmonitor_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, dirmonitor_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "wait", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, dirmonitor_wait, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "target", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, dirmonitor_target, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_FILEMONITOR_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_menu;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, menu_new, 0, -1, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, menu_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "add", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, menu_add, 2, 2, NULL, fs->cls_str, NULL);
    n = fs->define_identifier(m, cls, "add_separator", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, menu_add_separator, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_MENU_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_menuitem;
    cls->u.c.n_memb = INDEX_MENUITEM_NUM;
    fs->extends_method(cls, fs->cls_obj);


    // EventHandler
    cls = cls_evt_handler;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, evthandler_new, 0, 0, cls, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_dtor), 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LP], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, evthandler_call, 0, -1, NULL);
    n = fs->define_identifier(m, cls, "del", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, evthandler_del, 1, 1, NULL, fs->cls_fn);

    define_list_method_0(cls, "empty", NODEOPT_PROPERTY, m);
    define_list_method_0(cls, "size", NODEOPT_PROPERTY, m);
    define_list_method_1(cls, "push", fs->cls_fn, m);
    define_list_method_0(cls, "pop", 0, m);
    define_list_method_1(cls, "unshift", fs->cls_fn, m);
    define_list_method_0(cls, "shift", 0, m);
    define_list_method_0(cls, "clear", 0, m);

    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "GuiError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

static FoxtkStatic *newFoxtkStatic(void)
{
    FoxtkStatic *f = fs->Mem_get(&fg->st_mem, sizeof(FoxtkStatic));

    f->GuiHash_init = GuiHash_init;
    f->GuiHash_get_p = GuiHash_get_p;
    f->GuiHash_add_p = GuiHash_add_p;
    f->GuiHash_del_p = GuiHash_del_p;
    f->GuiHash_close = GuiHash_close;

    f->Value_handle = Value_handle;
    f->handle_Value = handle_Value;
    f->get_event_handler = get_event_handler;
    f->invoke_event_handler = invoke_event_handler;
    f->widget_event_handler_sub = widget_event_handler_sub;
    f->widget_handler_destroy = widget_handler_destroy;
    f->connect_widget_events = connect_widget_events;

    f->event_object_new = event_object_new;
    f->event_object_add = event_object_add;
    f->window_message_loop = window_message_loop;

    return f;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_gui = m;

    mod_image = fs->get_module_by_name("image", -1, TRUE, FALSE);
    define_class(m);
    define_func(m);

    gui_initialize();
    fs->add_unresolved_ptr(m, mod_image, "Image", &cls_image);
    m->u.m.ext = newFoxtkStatic();
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    if (buf == NULL) {
        char *ver = get_toolkit_version();
        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\nToolkit\t%s\n", ver);
        free(ver);
    }
    return buf;
}
