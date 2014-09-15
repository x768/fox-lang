#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"


MenuHandle Menu_new()
{
	return CreatePopupMenu();
}
MenuHandle Menubar_new()
{
	return CreateMenu();
}

void Window_set_menu(WndHandle wnd, MenuHandle menu)
{
	SetMenu(wnd, (HMENU)menu);
}
void Menu_add_submenu(MenuHandle menu, MenuHandle submenu, Str text)
{
	MENUITEMINFOW mii;
	wchar_t *text_p = cstr_to_utf16(text.p, text.size);

	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
	mii.fType = MFT_STRING;
	mii.hSubMenu = (HMENU)submenu;
	mii.dwTypeData = text_p;
	InsertMenuItemW((HMENU)menu, GetMenuItemCount((HMENU)menu), TRUE, &mii);
	free(text_p);
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

enum {
	MENU_ID_BEGIN = 100,
	MENU_ID_SIZE = 2048,
};

static Ref **menu_item_ids;

static int bind_menu_id(Ref *r)
{
	int i;

	if (menu_item_ids == NULL) {
		menu_item_ids = malloc(sizeof(Ref*) * MENU_ID_SIZE);
		memset(menu_item_ids, 0, sizeof(Ref*) * MENU_ID_SIZE);
	}
	for (i = 0; i < MENU_ID_SIZE; i++) {
		if (menu_item_ids[i] == NULL) {
			menu_item_ids[i] = r;
			return i + MENU_ID_BEGIN;
		}
	}
	return -1;
}
Ref *get_menu_object(int id)
{
	id -= MENU_ID_BEGIN;
	if (id >= 0 && id < MENU_ID_SIZE) {
		return menu_item_ids[id];
	} else {
		return NULL;
	}
}
void remove_menu_object(int id)
{
	id -= MENU_ID_BEGIN;
	if (id >= 0 && id < MENU_ID_SIZE) {
		menu_item_ids[id] = NULL;
	}
}

void Menu_add_item(MenuHandle menu, Value fn, Str text)
{
	int id;
	Ref *r = fs->new_ref(cls_menuitem);
	r->v[INDEX_MENUITEM_FN] = fs->Value_cp(fn);

	id = bind_menu_id(r);
	if (id >= 0) {
		MENUITEMINFOW mii;
		wchar_t *text_p = cstr_to_utf16(text.p, text.size);

		memset(&mii, 0, sizeof(mii));
		mii.cbSize = sizeof(mii);
		mii.wID = id;
		mii.fMask = MIIM_TYPE|MIIM_ID;
		mii.fType = MFT_STRING;
		mii.dwTypeData = text_p;
		InsertMenuItemW((HMENU)menu, GetMenuItemCount((HMENU)menu), TRUE, &mii);

		free(text_p);
	}
}
void Menu_add_separator(MenuHandle menu)
{
	MENUITEMINFOW mii;

	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE;
	mii.fType = MFT_SEPARATOR;
	InsertMenuItemW((HMENU)menu, GetMenuItemCount((HMENU)menu), TRUE, &mii);
}
void Menu_remove_all(MenuHandle menu)
{
}
