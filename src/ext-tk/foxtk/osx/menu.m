#define NOT_DEFINE_BOOL
#include "gui_compat.h"
#include "m_gui.h"


MenuHandle Menu_new()
{
    return NULL;
}
MenuHandle Menubar_new()
{
    return NULL;
}
void Window_set_menu(WndHandle wnd, MenuHandle menu)
{
}
void Menu_add_submenu(MenuHandle menu, MenuHandle submenu, const char *text)
{
}
void Menu_add_item(MenuHandle menu, Value *fn, const char *text)
{
}
void Menu_add_separator(MenuHandle menu)
{
}
void Menu_remove_all(MenuHandle menu)
{
}
