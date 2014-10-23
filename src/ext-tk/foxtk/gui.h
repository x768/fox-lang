#ifndef _GUI_H_
#define _GUI_H_

#include "m_gui.h"

enum {
    INDEX_FORM_MENU = INDEX_WIDGET_NUM,
    INDEX_FORM_DROP_HANDLE,
#ifdef WIN32
    INDEX_FORM_NEW_ICON,
    INDEX_FORM_ALPHA,
#endif // WIN32
    INDEX_FORM_NUM,
};
enum {
    INDEX_MENU_VALID,
    INDEX_MENU_HAS_PARENT,
    INDEX_MENU_HANDLE,
    INDEX_MENU_NUM,
};
enum {
    INDEX_MENUITEM_PARENT,
    INDEX_MENUITEM_FN,
    INDEX_MENUITEM_ID,
    INDEX_MENUITEM_NUM,
};
enum {
    INDEX_FILEMONITOR_STRUCT,
    INDEX_FILEMONITOR_FILE,
    INDEX_FILEMONITOR_FN,
    INDEX_FILEMONITOR_NUM,
};
enum {
    INDEX_TIMER_ID,
    INDEX_TIMER_FN,
    INDEX_TIMER_NUM,
};

enum {
    FILEOPEN_OPEN,
    FILEOPEN_OPEN_MULTI,
    FILEOPEN_SAVE,
    FILEOPEN_DIR,
};

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_gui;
extern RefNode *mod_image;

extern RefNode *cls_widget;
extern RefNode *cls_layout;
extern RefNode *cls_form;
extern RefNode *cls_tray_icon;
extern RefNode *cls_menu;
extern RefNode *cls_menuitem;
extern RefNode *cls_event;
extern RefNode *cls_evt_handler;
extern RefNode *cls_timer;
extern RefNode *cls_filemonitor;
extern RefNode *cls_image;
extern int root_window_count;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


// m_gui.c
void GuiHash_init(RefGuiHash *h);
Value *GuiHash_get_p(RefGuiHash *h, const void *key);
Value *GuiHash_add_p(RefGuiHash *h, const void *key);
void GuiHash_del_p(RefGuiHash *h, const void *key);
void GuiHash_close(RefGuiHash *h);

Value *get_event_handler(Ref *r, RefStr *name);
int invoke_event_handler(int *result, Value eh, Value evt);
void widget_event_handler_sub(Value *vret, Ref *r, RefStr *name);
int widget_handler_destroy(Ref *r);

Value event_object_new(Ref *sender_r);
void event_object_add(Value evt, const char *name, Value val);


// m_gui_???.c
void gui_initialize(void);
void gui_dispose(void);
char *get_toolkit_version(void);
void *Value_handle(Value v);
Value handle_Value(void *handle);


// dialog.c
void alert_dialog(RefStr *msg, RefStr *title, WndHandle parent);
int confirm_dialog(RefStr *msg, RefStr *title, WndHandle parent);
int file_open_dialog(Value *vret, Str title, RefArray *filter, WndHandle parent, int type);

// form.c
void create_form_window(Value *v, WndHandle parent, int *size);

void window_destroy(WndHandle window);
void window_set_drop_handler(WndHandle window);
int window_set_icon(WndHandle window, Ref *r, RefImage *icon);
double window_get_opacity(Ref *r);
void window_set_opacity(Ref *r, double opacity);
int window_get_client_size(WndHandle window, int height);
void window_set_client_size(WndHandle window, int width, int height);
void window_move(WndHandle window, int x, int y);
void window_move_center(WndHandle window);
void window_get_title(Value *vret, WndHandle window);
void window_set_title(WndHandle window, const char *s);
int window_get_visible(WndHandle window);
void window_set_visible(WndHandle window, int show);
void window_set_keep_above(WndHandle window, int above);
int window_message_loop(int *loop);

void connect_widget_events(WndHandle window);
void create_image_pane_window(Value *v, WndHandle parent);
int image_pane_set_image_sub(WndHandle window, RefImage *img);

void native_timer_new(int millisec, Ref *r);
void native_timer_remove(Ref *r);
int native_filemonitor_new(Ref *r, const char *path);
void native_filemonitor_remove(Ref *r);
void native_object_remove_all(void);


// clipboard.c
int conv_graphics_to_image(Value *vret, GraphicsHandle img);
GraphicsHandle conv_image_to_graphics(RefImage *img);
int clipboard_clear(void);
int clipboard_is_available(const RefNode *klass);
Value clipboard_get_text(void);
void clipboard_set_text(const char *src_p, int src_size);
int clipboard_get_files(Value *v, int uri);
int clipboard_set_files(Value *v, int num);
int clipboard_get_image(Value *v);
int clipboard_set_image(RefImage *img);

// menu.c
MenuHandle Menu_new(void);
MenuHandle Menubar_new(void);
int invoke_event(Value event, Value fn);
void Window_set_menu(WndHandle wnd, MenuHandle menu);
void Menu_add_submenu(MenuHandle menu, MenuHandle submenu, Str text);
void Menu_add_item(MenuHandle menu, Value fn, Str text);
void Menu_add_separator(MenuHandle menu);
#ifdef WIN32
Ref *get_menu_object(int id);
void remove_menu_object(int id);
#endif

// exio.c
int uri_to_file_sub(Value *dst, Value src);
int exio_trash_sub(Value vdst);


#endif /* _GUI_H_ */
