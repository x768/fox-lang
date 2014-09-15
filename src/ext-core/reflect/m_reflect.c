#define DEFINE_GLOBALS
#include "fox.h"


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_reflect;

static int ref_get_member(Value *vret, Value *v, RefNode *node)
{
	RefStr *name = Value_vp(v[2]);
	RefStr *name_r = fs->get_intern(name->c, name->size);

	if (name_r == NULL) {
		name_r = name;
	}
	if (name_r->size > 0 && name_r->c[0] == '_') {
		fs->throw_errorf(fs->mod_lang, "DefineError", "%r is private", name_r);
		return FALSE;
	}
	*fg->stk_top++ = fs->Value_cp(v[1]);

	if (!fs->call_property(name_r)) {
		return FALSE;
	}
	*vret = fg->stk_top[-1];
	fg->stk_top--;

	return TRUE;
}
static int ref_get_member_list(Value *vret, Value *v, RefNode *node)
{
	int name_only = FUNC_INT(node);
	RefNode *type = fs->Value_type(v[1]);
	RefMap *rm = fs->refmap_new(0);
	RefNode *nd_src;
	int i;

	if (name_only) {
		rm->rh.type = fs->cls_set;
	}
	*vret = vp_Value(rm);

	if (type == fs->cls_module || type == fs->cls_class) {
		nd_src = Value_vp(v[1]);
	} else {
		nd_src = type;
	}
	for (i = 0; i < nd_src->u.c.h.entry_num; i++) {
		HashEntry *entry;
		for (entry = nd_src->u.c.h.entry[i]; entry != NULL; entry = entry->next) {
			RefStr *name = entry->key;
			if (name->c[0] != '_') {
				RefNode *nd = entry->p;
				if (type == fs->cls_module) {
				} else if (type == fs->cls_class) {
					if ((nd->type & NODEMASK_MEMBER_S) == 0) {
						continue;
					}
				} else {
					if ((nd->type & NODEMASK_MEMBER) == 0) {
						continue;
					}
				}
				{
					HashValueEntry *e = fs->refmap_add(rm, vp_Value(name), FALSE, FALSE);
					if (!name_only) {
						fs->Value_push("v", v[1]);
						if (!fs->call_property(name)) {
							return FALSE;
						}
						e->val = fg->stk_top[-1];
						fg->stk_top--;
					}
				}
			}
		}
	}

	return TRUE;
}
static int get_declaring_class(Value *vret, Value *v, RefNode *node)
{
	Ref *fn_obj = Value_vp(v[1]);
	RefNode *fn;
	RefNode *cls;

	if (fn_obj->rh.n_memb > 0) {
		fn = Value_vp(fn_obj->v[INDEX_FUNC_FN]);
	} else {
		fn = Value_vp(v[1]);
	}

	cls = fn->u.f.klass;
	if (cls == NULL || cls->rh.type != fs->cls_class) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Not a class method");
		return FALSE;
	}
	if (cls != NULL){
		*vret = vp_Value(cls);
	}
	return TRUE;
}
static int get_declaring_module(Value *vret, Value *v, RefNode *node)
{
	RefNode *type = fs->Value_type(v[1]);
	RefNode *module;

	if (type == fs->cls_class) {
		RefNode *cls = Value_vp(v[1]);
		module = cls->defined_module;
	} else if (type == fs->cls_fn) {
		Ref *fn_obj = Value_vp(v[1]);
		if (fn_obj->rh.n_memb > 0) {
			RefNode *fn = Value_vp(fn_obj->v[INDEX_FUNC_FN]);
			module = fn->defined_module;
		} else {
			RefNode *fn = Value_vp(v[1]);
			module = fn->defined_module;
		}
	} else {
		fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_class, fs->cls_fn, type, 1);
		return FALSE;
	}
	if (module != NULL) {
		*vret = vp_Value(module);
	}
	return TRUE;
}
static int ref_get_this(Value *vret, Value *v, RefNode *node)
{
	Ref *fn_obj = Value_vp(v[1]);

	if (fn_obj->rh.n_memb == 0) {
		fs->throw_errorf(fs->mod_lang, "ValueError", "Not a class method");
		return FALSE;
	}
	*vret = fs->Value_cp(fn_obj->v[INDEX_FUNC_THIS]);
	return TRUE;
}
static int get_module_path(Value *vret, Value *v, RefNode *node)
{
	RefNode *module = Value_vp(v[1]);

	if (module->u.m.src_path != NULL) {
		*vret = fs->cstr_Value_conv(module->u.m.src_path, -1, NULL);
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
	RefNode *n;

	// get_member(v, 'hoge') == v.hoge
	n = fs->define_identifier(m, m, "get_member", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, ref_get_member, 2, 2, NULL, NULL, fs->cls_str);

	n = fs->define_identifier(m, m, "get_member_names", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, ref_get_member_list, 1, 1, (void*)TRUE, NULL);

	n = fs->define_identifier(m, m, "get_member_map", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, ref_get_member_list, 1, 1, (void*)FALSE, NULL);

	n = fs->define_identifier(m, m, "get_declaring_class", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, get_declaring_class, 1, 1, NULL, fs->cls_fn);

	n = fs->define_identifier(m, m, "get_declaring_module", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, get_declaring_module, 1, 1, NULL, NULL);

	n = fs->define_identifier(m, m, "get_this", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, ref_get_this, 1, 1, NULL, fs->cls_fn);

	n = fs->define_identifier(m, m, "get_module_path", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, get_module_path, 1, 1, NULL, fs->cls_module);
}
static void define_class(RefNode *m)
{
//	RefNode *cls;
//	RefNode *n;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_reflect = m;

	define_class(m);
	define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}
