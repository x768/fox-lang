#define DEFINE_GLOBALS
#include "fox.h"
#include "fox_io.h"
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>


static const FoxStatic *fs;
static FoxGlobal *fg;
static RefNode *mod_readline;



static int fox_readline(Value *vret, Value *v, RefNode *node)
{
	const char *prompt;
	char *line;

	// 標準出力のバッファをクリア
	fs->stream_flush_sub(fg->v_cio);

	if (fg->stk_top > v + 1) {
		RefStr *rs = Value_vp(v[1]);
		prompt = rs->c;
	} else {
		prompt = "";
	}
	line = readline(prompt);

	if (line != NULL) {
		*vret = fs->cstr_Value_conv(line, -1, NULL);
		free(line);
	}
	return TRUE;
}
static int fox_add_history(Value *vret, Value *v, RefNode *node)
{
	RefStr *rs = Value_vp(v[1]);
	add_history(rs->c);
	return TRUE;
}
static int fox_clear_history(Value *vret, Value *v, RefNode *node)
{
	clear_history();
	return TRUE;
}
static int fox_remove_history(Value *vret, Value *v, RefNode *node)
{
	int err = FALSE;
	int64_t idx = fs->Value_int(v[1], &err);

	if (!err && idx >= 0 && idx < INT32_MAX) {
		HIST_ENTRY *entry = remove_history(idx);
		if (entry != NULL) {
			free(entry->line);
			free(entry->data);
			free(entry);
		}
	}
	return TRUE;
}
static int fox_hisotry_list(Value *vret, Value *v, RefNode *node)
{
	int i;
	HIST_ENTRY **list = history_list();
	RefArray *ra = fs->refarray_new(0);

	*vret = vp_Value(ra);

	for (i = 0; list[i] != NULL; i++) {
		Value *vp = fs->refarray_push(ra);
		*vp = fs->cstr_Value_conv(list[i]->line, -1, NULL);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_function(RefNode *m)
{
	RefNode *n;

	n = fs->define_identifier(m, m, "readline", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, fox_readline, 0, 1, NULL, fs->cls_str);

	n = fs->define_identifier(m, m, "add_history", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, fox_add_history, 1, 1, NULL, fs->cls_str);

	n = fs->define_identifier(m, m, "clear_hisotry", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, fox_clear_history, 0, 0, NULL);

	n = fs->define_identifier(m, m, "remove_hisotry", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, fox_remove_history, 1, 1, NULL, fs->cls_int);

	n = fs->define_identifier(m, m, "hisotry_list", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, fox_hisotry_list, 0, 0, NULL);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_readline = m;

	define_function(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	static char *buf = NULL;
	
	if (buf == NULL) {
		buf = malloc(256);
		sprintf(buf, "Build at\t" __DATE__ "\nreadline\t%s\n",
				rl_library_version);
	}
	return buf;
}
