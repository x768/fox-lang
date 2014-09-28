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
		prompt = Value_cstr(v[1]);
	} else {
		prompt = "";
	}
	if (fs->cs_stdio != NULL && fs->cs_stdio != fs->cs_utf8) {
		IconvIO ic;
		StrBuf buf;

		if (!fs->IconvIO_open(&ic, fs->cs_utf8, fs->cs_stdio, "?")) {
			return FALSE;
		}
		fs->StrBuf_init(&buf, strlen(prompt));
		if (!fs->IconvIO_conv(&ic, &buf, prompt, -1, TRUE, TRUE)) {
			StrBuf_close(&buf);
			return FALSE;
		}
		fs->StrBuf_add_c(&buf, '\0');
		fs->IconvIO_close(&ic);

		line = readline(buf.p);
		StrBuf_close(&buf);
	} else {
		line = readline(prompt);
	}
	if (line != NULL) {
		*vret = fs->cstr_Value_conv(line, -1, NULL);
		free(line);
	}
	return TRUE;
}
static int fox_add_history(Value *vret, Value *v, RefNode *node)
{
	add_history(Value_cstr(v[1]));
	return TRUE;
}
static int fox_clear_history(Value *vret, Value *v, RefNode *node)
{
	clear_history();
	return TRUE;
}
#if 0
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
#endif

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

#if 0
	n = fs->define_identifier(m, m, "remove_hisotry", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, fox_remove_history, 1, 1, NULL, fs->cls_int);

	n = fs->define_identifier(m, m, "hisotry_list", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, fox_hisotry_list, 0, 0, NULL);
#endif
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
