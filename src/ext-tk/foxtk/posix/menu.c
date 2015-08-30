#include "gui_compat.h"
#include "gui.h"
#include <stdlib.h>


MenuHandle Menu_new()
{
    return gtk_menu_new();
}
MenuHandle Menubar_new()
{
    return gtk_menu_bar_new();
}

void Window_set_menu(WndHandle wnd, MenuHandle menu)
{
    gtk_container_add(GTK_CONTAINER(wnd), menu);
}
void Menu_add_submenu(MenuHandle menu, MenuHandle submenu, const char *text)
{
    GtkWidget *menu_item = gtk_menu_item_new_with_label(text);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
}

int invoke_event(Value event, Value fn)
{
    Value *stk_top = fg->stk_top;
    int ret_code = TRUE;

    fs->Value_push("vv", fn, event);
    ret_code = fs->call_function_obj(1);

    // エラーの場合、stk_topが不定
    while (fg->stk_top > stk_top) {
        fs->Value_pop();
    }
    return ret_code;
}
static void signal_menu_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    Ref *sender_r = (Ref*)user_data;
    Value evt = event_object_new(sender_r);
    invoke_event(evt, sender_r->v[INDEX_MENUITEM_FN]);
}

void Menu_add_item(MenuHandle menu, Value fn, const char *text)
{
    GtkWidget *item;
    Ref *r = fs->ref_new(cls_menuitem);

    r->v[INDEX_MENUITEM_FN] = fs->Value_cp(fn);

    item = gtk_menu_item_new_with_label(text);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(signal_menu_activate), r);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}
void Menu_add_separator(MenuHandle menu)
{
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}
void Menu_remove_all(MenuHandle menu)
{
}
