#ifndef _M_GUI_H_
#define _M_GUI_H_

#include "m_image.h"

typedef void *WndHandle;
typedef void *MenuHandle;
typedef void *GraphicsHandle;

/**
 * WidgetやTimerなど
 */
enum {
    INDEX_WBASE_HANDLER,
#ifdef WIN32
    INDEX_WBASE_CALLBACK,
#endif
    INDEX_WBASE_NUM,
};
/**
 * Widget系
 */
enum {
    INDEX_WIDGET_HANDLE = INDEX_WBASE_NUM,  // Windowsの場合はRefHWndの参照
    INDEX_WIDGET_PARENT,
    INDEX_WIDGET_GENERAL_USE,
    INDEX_WIDGET_NUM,
};

enum {
    GUI_ENTRY_NUM = 64,
};

typedef struct GuiEntry {
    struct GuiEntry *next;
    const void *key;
    Value val;
} GuiEntry;

typedef struct {
    RefHeader rh;

    GuiEntry *entry[GUI_ENTRY_NUM];
} RefGuiHash;

typedef struct {
    void (*GuiHash_init)(RefGuiHash *h);
    Value *(*GuiHash_get_p)(RefGuiHash *h, const void *key);
    Value *(*GuiHash_add_p)(RefGuiHash *h, const void *key);
    void (*GuiHash_del_p)(RefGuiHash *h, const void *key);
    void (*GuiHash_close)(RefGuiHash *h);

    void *(*Value_handle)(Value v);
    Value (*handle_Value)(void *handle);
    Value *(*get_event_handler)(Ref *r, RefStr *name);
    int (*invoke_event_handler)(int *result, Value eh, Value evt);
    void (*widget_event_handler_sub)(Value *vret, Ref *r, RefStr *name);
    int (*widget_handler_destroy)(Ref *r);
    void (*connect_widget_events)(WndHandle window);

    Value (*event_object_new)(Ref *sender_r);
    void (*event_object_add)(Value evt, const char *name, Value val);
    int (*window_message_loop)(void);
} FoxtkStatic;


#endif /* _M_GUI_H_ */
