#define DEFINE_GLOBALS
#include "m_editor.h"
#include <string.h>
#include <stdlib.h>


static EditWnd *newEditWnd()
{
	EditWnd *w = malloc(sizeof(EditWnd));
	memset(w, 0, sizeof(EditWnd));
	return w;
}

static int editor_pane_new(Value *vret, Value *v, RefNode *node)
{
	Ref *r = Value_vp(v[1]);
	WndHandle parent = ftk->Value_handle(r->v[INDEX_WIDGET_HANDLE]);

	*vret = vp_Value(fs->new_ref(cls_editor));
	create_editor_pane_window(vret, parent);
	r->v[INDEX_EDITWND_DATA] = ptr_Value(newEditWnd());

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

	cls_editor = fs->define_identifier(m, m, "EditorPane", NODE_CLASS, 0);

	cls = cls_editor;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, editor_pane_new, 0, 1, cls, NULL);
	fs->add_unresolved_args(m, mod_gui, "Layout", n, 0);

	cls->u.c.n_memb = INDEX_EDITWND_NUM;
	fs->add_unresolved_module_p(m, "Widget", mod_gui, cls);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_editor = m;

	mod_gui = fs->get_module_by_name("foxtk", -1, TRUE, FALSE);
	ftk = mod_gui->u.m.ext;
	define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}
