#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"


static wchar_t *value_to_wfile(Value v)
{
	const RefNode *type = fs->Value_type(v);

	if (type == fs->cls_str || type == fs->cls_file) {
		RefStr *rs = Value_vp(v);
		return cstr_to_utf16(rs->c, rs->size);
	} else if (type == fs->cls_uri) {
		wchar_t *wp;
		RefStr *rs;
		fs->Value_push("v", v);
		if (!fs->call_member_func(fs->intern("to_file", -1), 0, TRUE)) {
			return NULL;
		}
		rs = Value_vp(fg->stk_top[-1]);
		wp = cstr_to_utf16(rs->c, rs->size);
		fs->Value_pop();
		return wp;
	} else {
		fs->throw_errorf(fs->mod_lang, "TypeError", "Str, File or Uri required but %n", type);
		return NULL;
	}
}


int uri_to_file_sub(Value *dst, Value src)
{
	fs->Value_push("v", src);
	if (!fs->call_member_func(fs->intern("to_file", -1), 0, TRUE)) {
		return FALSE;
	}
	fg->stk_top--;
	*dst = *fg->stk_top;
	return TRUE;
}

int exio_trash_sub(Value vdst)
{
	SHFILEOPSTRUCTW sfo;
	int result;
	wchar_t *dst = value_to_wfile(vdst);

	if (dst == NULL) {
		fs->throw_errorf(fs->mod_io, "IOError", "Cannot delete file");
		return FALSE;
	}
	memset(&sfo, 0, sizeof(sfo));
	sfo.wFunc = FO_DELETE;
	sfo.pFrom = dst;
	sfo.fFlags = FOF_ALLOWUNDO|FOF_NOCONFIRMATION|FOF_NOERRORUI;

	result = SHFileOperationW(&sfo);
	free(dst);

	if (result != 0) {
		fs->throw_errorf(fs->mod_io, "IOError", "Cannot delete file");
		return FALSE;
	}
	return TRUE;
}
