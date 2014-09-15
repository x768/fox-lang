#define DEFINE_GLOBALS
#include "fox.h"
#include "fox_io.h"


static const FoxStatic *fs;
static FoxGlobal *fg;
static RefNode *mod_osx;


////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

/*
	cls = fs->define_identifier(m, m, "LzmaIO", NODE_CLASS, 0);
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, lzmaio_new, 1, 3, cls, fs->cls_streamio, fs->cls_int, fs->cls_str);

	n = fs->define_identifier(m, cls, "_close", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, lzmaio_close, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, lzmaio_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
	n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, lzmaio_write, 1, 1, NULL, fs->cls_bytesio);

	cls->u.c.n_memb = INDEX_LZMA_NUM;
	fs->extends_method(cls, fs->cls_streamio);


	cls = fs->define_identifier(m, m, "LzmaError", NODE_CLASS, 0);
	cls->u.c.n_memb = 2;
	fs->extends_method(cls, fs->cls_error);
*/
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_osx = m;

	define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	static char *buf = NULL;
	
	if (buf == NULL) {
		buf = malloc(256);
		sprintf(buf, "Build at\t" __DATE__ "\n");
	}
	return buf;
}
