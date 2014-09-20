#include "fox.h"
#include <string.h>
#include <stdlib.h>



enum {
	MD_TEXT,
};


typedef struct MDNode {
	struct MDNode *child;
	struct MDNode *next;

	int type;
	const char *text;
	const char *title;
	const char *link;
} MDNode;

typedef struct {
	Mem mem;
	int enable_semantic;
	Hash link_map;
	MDNode *root;

	Value plugin_callback;
} RefMarkdown;

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_markdown;
static RefNode *mod_xml;


static void add_markdown_source(RefMarkdown *r, const char *p, const char *end)
{
}


//////////////////////////////////////////////////////////////////////////////////////////

static int markdown_new(Value *vret, Value *v, RefNode *node)
{
	RefNode *cls_markdown = FUNC_VP(node);
	RefMarkdown *r = fs->new_buf(cls_markdown, sizeof(RefMarkdown));

	fs->Mem_init(&r->mem, 1024);
	*vret = vp_Value(r);
	return TRUE;
}
static int markdown_dispose(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	fs->Mem_close(&r->mem);
	fs->Value_dec(r->plugin_callback);
	return TRUE;
}
static int markdown_plugin_callback(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	if (fg->stk_top > v + 1) {
		r->plugin_callback = fs->Value_cp(v[1]);
	} else {
		*vret = fs->Value_cp(r->plugin_callback);
	}
	return TRUE;
}
static int markdown_enable_semantic(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	if (fg->stk_top > v + 1) {
		r->enable_semantic = Value_bool(v[1]);
	} else {
		*vret = bool_Value(r->enable_semantic);
	}
	return TRUE;
}

static int markdown_add_src(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	RefStr *rs = Value_vp(v[1]);
	const char *top = rs->c;
	const char *p = top;
	const char *end = top + rs->size;

	while (p < end) {
		while (p < end && *p != '\n') {
			p++;
		}
		add_markdown_source(r, top, p);
		if (p < end) {
			p++;
		}
	}

	return TRUE;
}
static int markdown_make(Value *vret, Value *v, RefNode *node)
{
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
	RefNode *n;
	RefNode *cls;

	cls = fs->define_identifier(m, m, "Markdown", NODE_CLASS, 0);
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, markdown_new, 0, 0, cls);

	n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_NEW_N, 0);
	fs->define_native_func_a(n, markdown_dispose, 0, 0, cls);
	n = fs->define_identifier(m, cls, "plugin_callback", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, markdown_plugin_callback, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "plugin_callback=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_plugin_callback, 1, 1, NULL, fs->cls_fn);
	n = fs->define_identifier(m, cls, "enable_semantic", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, markdown_enable_semantic, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "enable_semantic=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_enable_semantic, 1, 1, NULL, fs->cls_bool);

	n = fs->define_identifier(m, cls, "add_source", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_add_src, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier(m, cls, "make", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_make, 0, 0, NULL);
	fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_markdown = m;
	mod_xml = fs->get_module_by_name("markup.xml", -1, TRUE, FALSE);

	define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}
