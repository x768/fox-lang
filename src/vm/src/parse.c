#include "fox_parse.h"

#ifndef WIN32
#include <dlfcn.h>
#endif


#ifdef WIN32
#define PATH_SPLIT_C ';'
#else
#define PATH_SPLIT_C ':'
#endif



typedef struct OpBuf
{
	OpCode *p;
	int cur, max, prev;
	int n_stack;
} OpBuf;


static const char *token_name[] = {
	"[EOF]", "T_ERR", "(", ")", "{", "}", "[", "]", "]=", ",", ";", ".", "[\\n]", "(", "[",
	"(Pragma)", "(Int)", "(Int)", "(Float)", "(Rational)", "(Str)", "(Bytes)", "(Regex)", "(Class)", "(var)", "(const)",
	"(Str)", "(Str)", "(Str)", "(Str)",

	"==", "!=", "<=>", "<", "<=", ">", ">=", ":", "?",
	"+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^",
	"&&", "||", "..", "...", "=>", "~X", "!X", "+X", "-X",

	"=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=", "++", "--", "in",

	"abstract", "break", "case", "catch", "class", "continue", "def", "default", "else", "elif", "false",
	"for", "if", "import", "in", "let", "null", "return", "super", "switch", "this", "throw",
	"true", "try", "var", "while", "yield",
};

////////////////////////////////////////////////////////////////////////////////

static void unexpected_token_error(Tok *tk)
{
	if (tk->v.type != T_ERR) {
		if (tk->v.type >= TL_CLASS && tk->v.type <= TL_CONST) {
			throw_errorf(fs->mod_lang, "SyntaxError", "Unexpected token %s %Q", token_name[tk->v.type], tk->str_val);
		} else {
			throw_errorf(fs->mod_lang, "SyntaxError", "Unexpected token %s", token_name[tk->v.type]);
		}
		add_stack_trace(tk->module, NULL, tk->v.line);
	}
}
static void already_defined_name_error(Tok *tk, RefNode *node)
{
	if (node->defined_module != NULL) {
		throw_errorf(fs->mod_lang, "DefineError", "Already defined name %n at line %d", node, node->defined_line);
	} else {
		throw_errorf(fs->mod_lang, "DefineError", "Already defined name %n", node);
	}
	add_stack_trace(tk->module, NULL, tk->v.line);
}
static void module_not_exist_error(Tok *tk, Str name)
{
	throw_errorf(fs->mod_lang, "NameError", "Module %S is not exist", name);
	add_stack_trace(tk->module, NULL, tk->v.line);
}
static void syntax_error(Tok *tk, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	throw_error_vprintf(fs->mod_lang, "SyntaxError", fmt, va);
	va_end(va);

	add_stack_trace(tk->module, NULL, tk->v.line);
}
static void define_error(Tok *tk, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	throw_error_vprintf(fs->mod_lang, "DefineError", fmt, va);
	va_end(va);

	add_stack_trace(tk->module, NULL, tk->v.line);
}

////////////////////////////////////////////////////////////////////////////////

static void OpBuf_init(OpBuf *buf)
{
	buf->cur = 0;
	buf->prev = -1;
	buf->max = 32;
	buf->n_stack = 0;
	buf->p = malloc(sizeof(OpCode) * buf->max);
}
static void OpBuf_close(OpBuf *buf)
{
	free(buf->p);
}
static OpCode *OpBuf_fix(OpBuf *buf, Mem *mem)
{
	OpCode *ret = Mem_get(mem, sizeof(OpCode) * buf->cur);
	memcpy(ret, buf->p, sizeof(OpCode) * buf->cur);

	return ret;
}
static OpCode *OpBuf_prev(OpBuf *buf)
{
	if (buf->prev < 0) {
		fatal_errorf(NULL, "OpBuf_prev:cannot rewind");
	}
	return buf->p + buf->prev;
}
static int OpBuf_add_op1(OpBuf *buf, int32_t type, int32_t s)
{
	enum {
		OP_SIZE = 1,
	};
	OpCode *op;

	if (buf->max <= buf->cur + OP_SIZE) {
		buf->max *= 2;
		buf->p = realloc(buf->p, sizeof(OpCode) * buf->max);
	}

	op = &buf->p[buf->cur];
	op->type = type;
	op->s = s;

	buf->prev = buf->cur;
	buf->cur += OP_SIZE;

	switch (type) {
	case OP_GET_LOCAL:
		buf->n_stack++;
		break;
	case OP_DUP:
		buf->n_stack += s;
		break;
	}

	return buf->prev;
}
static int OpBuf_add_op2(OpBuf *buf, int32_t type, int32_t s, Value v1)
{
	enum {
		OP_SIZE = 2,
	};
	OpCode *op;

	if (buf->max <= buf->cur + OP_SIZE) {
		buf->max *= 2;
		buf->p = realloc(buf->p, sizeof(OpCode) * buf->max);
	}

	op = &buf->p[buf->cur];
	op->type = type;
	op->s = s;
	op->op[0] = v1;

	buf->prev = buf->cur;
	buf->cur += OP_SIZE;

	switch (type) {
	case OP_GET_FIELD:
	case OP_NEW_FN:
	case OP_MODULE:
	case OP_CALL:
	case OP_CALL_POP:
	case OP_CALL_ITER:
	case OP_RANGE_NEW:
	case OP_LITERAL:
		buf->n_stack++;
		break;
	}

	return buf->prev;
}
static int OpBuf_add_op3(OpBuf *buf, int32_t type, int32_t s, Value v1, Value v2)
{
	enum {
		OP_SIZE = 3,
	};
	OpCode *op;

	if (buf->max <= buf->cur + OP_SIZE) {
		buf->max *= 2;
		buf->p = realloc(buf->p, sizeof(OpCode) * buf->max);
	}

	op = &buf->p[buf->cur];
	op->type = type;
	op->s = s;
	op->op[0] = v1;
	op->op[1] = v2;

	buf->prev = buf->cur;
	buf->cur += OP_SIZE;

	switch (type) {
	case OP_LITERAL_P:
	case OP_CALL_M:
	case OP_CALL_M_POP:
	case OP_GET_PROP:
	case OP_PUSH_CATCH:
		buf->n_stack++;
		break;
	}

	return buf->prev;
}
static void OpBuf_grow(OpBuf *buf, int n)
{
	if (buf->max <= buf->cur + n) {
		buf->max *= 2;
		buf->p = realloc(buf->p, sizeof(OpCode) * buf->max);
	}
	buf->cur += n;
}
static void OpBuf_add_str(OpBuf *buf, Tok *tk)
{
	OpBuf_add_op2(buf, OP_LITERAL, 0, cstr_Value(fs->cls_str, tk->str_val.p, tk->str_val.size));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static RefNode *new_Module(int native)
{
	RefNode *m = Mem_get(&fg->st_mem, sizeof(RefNode));
	memset(m, 0, sizeof(*m));

	m->u.m.src_path = NULL;
	Hash_init(&m->u.m.h, &fg->st_mem, 32);
	if (!native) {
		Hash_init(&m->u.m.import, &fg->st_mem, 16);
		Hash_init(&m->u.m.unresolved, &fv->cmp_mem, 32);
	}
	m->rh.type = fs->cls_module;
	m->rh.nref = -1;
	m->type = NODE_MODULE;

	return m;
}
static RefNode *Node_new_localvar(Mem *mem, Tok *tk, RefStr *name, int opt)
{
	RefNode *node = Mem_get(mem, sizeof(RefNode));

	node->type = NODE_VAR;
	node->opt = opt;
	node->name = name;
	node->defined_module = tk->module;
	node->defined_line = tk->v.line;

	return node;
}

/**
 * エラーの場合はNULLを返す
 */
RefNode *Node_define(RefNode *root, RefNode *module, RefStr *name, int type, RefNode *node)
{
	HashEntry *entry = Hash_get_add_entry(&root->u.c.h, &fg->st_mem, name);

	if (entry->p != NULL) {
		node = entry->p;
	} else {
		if (node == NULL) {
			node = Mem_get(&fg->st_mem, sizeof(RefNode));
		}
		node->uid = NULL;
		entry->p = node;
	}

	node->rh.nref = -1;
	node->rh.n_memb = 0;
	node->rh.weak_ref = NULL;

	node->type = type;
	node->opt = 0;
	node->name = name;
	node->defined_module = module;
	node->defined_line = 0;

	if (type == NODE_CLASS) {
		node->rh.type = fs->cls_class;
		Hash_init(&node->u.c.h, &fg->st_mem, 32);
		node->u.c.super = fs->cls_obj;
		node->u.c.n_offset = 0;
		node->u.c.n_memb = 0;
	} else {
		node->rh.type = fs->cls_fn;
	}

	return node;
}
static RefNode *Node_define_tk(RefNode *root, Tok *tk, RefStr *name, int type)
{
	HashEntry *entry = Hash_get_add_entry(&root->u.c.h, &fg->st_mem, name);
	RefNode *node;

	if (entry->p != NULL) {
		node = entry->p;
		if (node->type != NODE_UNRESOLVED) {
			already_defined_name_error(tk, node);
			return NULL;
		}
	} else {
		node = Mem_get(&fg->st_mem, sizeof(RefNode));
		node->uid = NULL;
		entry->p = node;
	}

	node->rh.nref = -1;
	node->rh.n_memb = 0;
	node->rh.weak_ref = NULL;

	node->type = type;
	node->opt = 0;
	node->name = name;
	node->defined_module = tk->module;
	node->defined_line = tk->v.line;

	switch (type) {
	case NODE_CLASS:
		node->rh.type = fs->cls_class;
		Hash_init(&node->u.c.h, &fg->st_mem, 32);
		node->u.c.super = fs->cls_obj;
		node->u.c.n_offset = 0;
		node->u.c.n_memb = 0;
		break;
	case NODE_FUNC:
		node->rh.type = fs->cls_fn;
		node->u.f.max_stack = 1;
		break;
	default:
		node->rh.type = fs->cls_fn;
		break;
	}

	return node;
}
// module: tk->module
// line:   tk->v.line
static UnresolvedID *Node_add_unresolve(RefNode *root, RefNode *module, int line, RefStr *name)
{
	HashEntry *entry = Hash_get_add_entry(&root->u.c.h, &fg->st_mem, name);
	RefNode *node;

	if (entry->p != NULL) {
		node = entry->p;
	} else {
		node = Mem_get(&fg->st_mem, sizeof(RefNode));

		node->type = NODE_UNRESOLVED;
		node->name = name;
		node->uid = NULL;
		entry->p = node;
	}
	if (node->uid == NULL) {
		UnresolvedID *uid = Mem_get(&fv->cmp_mem, sizeof(*uid));
		uid->ref_module = module;
		uid->ref_line = line;
		uid->ur = NULL;

		node->uid = uid;
	}

	return node->uid;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * トップレベル識別子を参照した場合
 *
 * ex)
 * func()
 * CONSTANT
 * SomeClass.new()
 */
static void add_unresolved_top(Tok *tk, int type, RefStr *name, RefNode *node, int pc)
{
	HashEntry *entry = Hash_get_add_entry(&tk->module->u.m.unresolved, &fv->cmp_mem, name);
	UnresolvedID *uid = entry->p;

	if (uid == NULL) {
		uid = Mem_get(&fv->cmp_mem, sizeof(UnresolvedID));
		uid->ref_module = tk->module;
		uid->ref_line = tk->v.line;
		uid->ur = NULL;
		entry->p = uid;
	}

	{
		Unresolved *p = Mem_get(&fv->cmp_mem, sizeof(Unresolved));
		p->type = type;
		p->u.node = node;
		p->pc = pc;

		// 末尾に追加
		p->next = uid->ur;
		uid->ur = p;
	}
}
/**
 * 名前空間を明示して参照した場合
 *
 * ex)
 * math.sin(1.0)
 * foo.CONSTANT
 * hoge.MyClass.open()
 */
static void add_unresolved_module(Tok *tk, int type, RefStr *name, RefNode *module, RefNode *node, int pc)
{
	UnresolvedID *uid = Node_add_unresolve(module, tk->module, tk->v.line, name);
	Unresolved *p = Mem_get(&fv->cmp_mem, sizeof(Unresolved));

	p->type = type;
	p->u.node = node;
	p->pc = pc;

	// 末尾に追加
	p->next = uid->ur;
	uid->ur = p;
}
// mod_src: エラーメッセージで場所を示すためのmodule
// name: 継承元クラス名
// module: 継承元クラスの場所
// node: 継承先クラス
void add_unresolved_module_p(RefNode *mod_src, const char *name, RefNode *module, RefNode *node)
{
	UnresolvedID *uid = Node_add_unresolve(module, mod_src, 0, intern(name, -1));
	Unresolved *p = Mem_get(&fv->cmp_mem, sizeof(Unresolved));

	p->type = UNRESOLVED_EXTENDS;
	p->u.node = node;
	p->pc = 0;

	// 末尾に追加
	p->next = uid->ur;
	uid->ur = p;
}

static void add_unresolved_memb(Tok *tk, RefNode *fn, RefStr *memb, int pc)
{
	UnrslvMemb *um;

	if (fv->unresolved_memb_n >= MAX_UNRSLV_NUM) {
		UnresolvedMemb *ur = Mem_get(&fv->cmp_mem, sizeof(UnresolvedMemb));
		ur->next = NULL;
		fv->unresolved_memb->next = ur;
		fv->unresolved_memb = ur;
		fv->unresolved_memb_n = 0;
		um = &ur->rslv[fv->unresolved_memb_n];
	} else {
		UnresolvedMemb *ur = fv->unresolved_memb;
		um = &ur->rslv[fv->unresolved_memb_n];
	}
	fv->unresolved_memb_n++;
	um->fn = fn;
	um->line = tk->v.line;
	um->memb = memb;
	um->pc = pc;
}

////////////////////////////////////////////////////////////////////////////////

Block *Block_new(Block *parent, RefNode *func)
{
#ifdef DEBUGGER
	Block *b = Mem_get(NULL, sizeof(Block));
	Hash_init(&b->h, NULL, 16);
#else
	Block *b = Mem_get(&fv->cmp_mem, sizeof(Block));
	Hash_init(&b->h, &fv->cmp_mem, 16);
#endif
	b->parent = parent;
	b->break_p = -1;
	b->continue_p = -1;

	if (parent != NULL) {
		b->offset = parent->offset;
		b->func = parent->func;
		b->import = parent->import;
	} else if (func != NULL) {
		b->offset = 1; // 暗黙のthis
		b->func = func;
		b->import = &func->defined_module->u.m.import;
	}
	return b;
}
static RefNode *Block_get_var(Block *bk, RefStr *name)
{
	RefNode *node;
	Block *b = bk;

	while (b != NULL) {
		if (b->h.count > 0) {
			node = Hash_get_p(&b->h, name);
			if (node != NULL) {
				return node;
			}
		}
		b = b->parent;
	}

	return NULL;
}
static RefNode *Block_add_var(Block *bk, Tok *tk, RefStr *name, int opt)
{
	RefNode *node = Block_get_var(bk, name);

	if (node != NULL) {
		already_defined_name_error(tk, node);
		return NULL;
	}

	// nodeは実行時に必要
	node = Node_new_localvar(&fg->st_mem, tk, name, opt);
	node->u.v.offset = bk->offset;
	node->u.v.var = FALSE;
	bk->offset++;

#ifdef DEBUGGER
	Hash_add_p(&bk->h, tk->cmp_mem, name, node);
#else
	Hash_add_p(&bk->h, &fv->cmp_mem, name, node);
#endif

	return node;
}

static int parse_import(RefNode *module, Tok *tk)
{
	RefNode *mod = NULL;
	RefStr *alias = NULL;
	char *name;
	char *name_ptr;

	Tok_next(tk);
	name = (char*)tk->str_val.p;
	name_ptr = name;

	for (;;) {
		if (tk->v.type == TL_VAR) {
			if (name_ptr < tk->str_val.p) {
				memmove(name_ptr, tk->str_val.p, tk->str_val.size);
			}
			name_ptr += tk->str_val.size;
			Tok_next(tk);
		} else {
			unexpected_token_error(tk);
			return FALSE;
		}
		if (tk->v.type == T_MEMB) {
			if (name_ptr < tk->str_val.p) {
				*name_ptr = '.';
			}
			name_ptr++;
			Tok_next(tk);
		} else {
			break;
		}
	}

	if (tk->v.type == T_ARROW) {
		Tok_next(tk);
		if (tk->v.type == TL_VAR) {
			RefNode *node;
			alias = Tok_intern(tk);

			// aliasが既に登録されいないか調べる
			node = Hash_get_p(&module->u.m.h, alias);
			if (node != NULL) {
				already_defined_name_error(tk, node);
				return FALSE;
			}
		} else {
			unexpected_token_error(tk);
			return FALSE;
		}
		Tok_next(tk);
	} else {
		alias = NULL;
	}

	mod = get_module_by_name(name, name_ptr - name, FALSE, FALSE);

	if (mod != NULL) {
		if (fg->error != VALUE_NULL) {
			return FALSE;
		}
	} else {
		// すでに例外は送出されている
		add_stack_trace(tk->module, NULL, tk->v.line);
		return FALSE;
	}

	// エラーメッセージ出力のため(別ファイルから上書きされる)
	mod->defined_module = module;
	mod->defined_line = tk->v.line;

	if (alias != NULL) {
		Hash_add_p(&module->u.m.import, &fg->st_mem, alias, mod);
	} else {
		// すでに登録されていたらエラー
		PtrList *pl;
		for (pl = module->u.m.usng; pl != NULL; pl = pl->next) {
			if (pl->u.p == mod) {
				throw_errorf(fs->mod_lang, "ImportError", "Duplcate import %S", Str_new(name, name_ptr - name));
				add_stack_trace(tk->module, NULL, tk->v.line);
				return FALSE;
			}
		}
		PtrList_add_p(&module->u.m.usng, mod, &fg->st_mem);
	}

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////

static int parse_expr(OpBuf *buf, Block *bk, Tok *tk);
static int parse_args(RefNode *node, Tok *tk, Block *bk);
static int parse_body_ep(RefNode *node, Tok *tk, Block *bk);

static int parse_closure(OpBuf *buf, Block *bk, Tok *tk)
{
	Block *bk2;

	RefNode *node = Mem_get(&fg->st_mem, sizeof(RefNode));
	node->rh.type = fs->cls_fn;
	node->rh.n_memb = 0;
	node->rh.nref = 0;
	node->rh.weak_ref = NULL;

	node->uid = NULL;
	node->u.f.klass = NULL;
	node->type = NODE_FUNC;
	node->opt = 0;
	node->name = fs->str_anonymous;
	node->defined_module = tk->module;
	node->defined_line = tk->v.line;

	bk2 = Block_new(bk, node);
	bk2->func = node;

	if (tk->v.type == T_LP_C || tk->v.type == T_LP) {
		if (!parse_args(node, tk, bk2)) {
			return FALSE;
		}
	} else if (tk->v.type == TL_VAR) {
		RefNode *var;
		RefStr *name = Tok_intern(tk);
		if (tk->parse_cls != NULL) {
			var = Hash_get_p(&tk->m_var, name);
			if (var != NULL) {
				already_defined_name_error(tk, var);
				return FALSE;
			}
		}
		var = Block_add_var(bk2, tk, name, NODEOPT_READONLY);
		if (var == NULL) {
			return FALSE;
		}

		Tok_next(tk);

		node->u.f.arg_min = 1;
		node->u.f.arg_max = 1;
		node->u.f.arg_type = NULL;
	}
	if (tk->v.type != T_ARROW) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next(tk);

	if (!parse_body_ep(node, tk, bk2)) {
		return FALSE;
	}

	{
		int fn = OpBuf_add_op2(buf, OP_NEW_FN, 0, vp_Value(node));
		OpCode *op;
		int offset = 0;
		int ths = FALSE;   // thisが使われている
		int off1 = bk->offset;             // 外部変数と引数の境目
		int off2 = bk2->offset - off1 + 1; // ローカル変数参照のoffset

		uint16_t *tr = malloc(sizeof(uint16_t) * (bk->offset + 1));
		memset(tr, 0, sizeof(uint16_t) * (bk->offset + 1));

		// ローカル変数参照を書き換える
		op = node->u.f.u.op;
		while (op->type != OP_RETURN_VAL) {
			if (op->type == OP_GET_LOCAL || op->type == OP_GET_LOCAL_V || op->type == OP_LOCAL_FN) {
				if (op->s == 0) {
					ths = TRUE;
				} else if (op->s < off1) {
					// 外部変数参照
					if (tr[op->s] != 0) {
						op->s = tr[op->s];
					} else {
						OpBuf_add_op2(buf, OP_LOCAL_FN, op->s, (Value)offset);
						tr[op->s] = offset + off2;
						op->s = offset + off2;
						offset++;
					}
				} else {
					// 引数(0番目はthisが入る)
					op->s -= off1 - 1;
				}
			} else if (op->type == OP_GET_FIELD) {
				//TODO
				ths = TRUE;
			}
			if (op->type > OP_SIZE_3) {
				op += 3;
			} else if (op->type > OP_SIZE_2) {
				op += 2;
			} else {
				op += 1;
			}
		}
		free(tr);
		if (offset > 0 || ths) {
			buf->p[fn].s = offset;
		} else {
			buf->p[fn].type = OP_FUNC;
		}
	}

	return TRUE;
}
static int parse_elem(OpBuf *buf, Block *bk, Tok *tk)
{
	switch (tk->v.type) {
	case TT_THIS:
		if (tk->parse_cls == NULL) {
			syntax_error(tk, "'this' is not allowed here.");
			return FALSE;
		}
		OpBuf_add_op1(buf, OP_GET_LOCAL, 0);
		Tok_next(tk);
		break;
	case TL_INT: {
		OpBuf_add_op2(buf, OP_LITERAL, 0, int32_Value(tk->int_val));
		Tok_next(tk);
		break;
	}
	case TL_MPINT: {
		RefInt *mp = new_buf(fs->cls_int, sizeof(RefInt));
		Value v = vp_Value(mp);
		mp->mp = tk->mp_val[0];
		OpBuf_add_op2(buf, OP_LITERAL, 0, v);
		Tok_next(tk);
		break;
	}
	case TL_FRACTION: {
		RefFrac *md = new_buf(fs->cls_frac, sizeof(RefFrac));
		Value v = vp_Value(md);
		md->md[0] = tk->mp_val[0];
		md->md[1] = tk->mp_val[1];
		OpBuf_add_op2(buf, OP_LITERAL, 0, v);
		Tok_next(tk);
		break;
	}
	case TL_FLOAT: {
		RefFloat *rd = new_buf(fs->cls_float, sizeof(RefFloat));
		Value v = vp_Value(rd);
		rd->d = tk->real_val;
		OpBuf_add_op2(buf, OP_LITERAL, 0, v);
		Tok_next(tk);
		break;
	}
	case TL_REGEX: {
		Ref *r = new_ref(fs->cls_regex);
		Value v = vp_Value(r);
		r->v[INDEX_REGEX_PTR] = ptr_Value(tk->re_val);
		r->v[INDEX_REGEX_SRC] = cstr_Value(fs->cls_str, tk->str_val.p, tk->str_val.size);
		r->v[INDEX_REGEX_OPT] = int32_Value(tk->int_val);
		OpBuf_add_op2(buf, OP_LITERAL, 0, v);
		Tok_next(tk);
		break;
	}
	case TL_STR:
		OpBuf_add_str(buf, tk);
		Tok_next(tk);
		break;
	case TL_BIN: {
		Value v = cstr_Value(fs->cls_bytes, tk->str_val.p, tk->str_val.size);
		OpBuf_add_op2(buf, OP_LITERAL, 0, v);
		Tok_next(tk);
		break;
	}
	case TT_TRUE:
		OpBuf_add_op2(buf, OP_LITERAL, 0, VALUE_TRUE);
		Tok_next(tk);
		break;
	case TT_FALSE:
		OpBuf_add_op2(buf, OP_LITERAL, 0, VALUE_FALSE);
		Tok_next(tk);
		break;
	case TT_NULL:
		OpBuf_add_op2(buf, OP_LITERAL, 0, VALUE_NULL);
		Tok_next(tk);
		break;
	case TL_STRCAT_BEGIN: {
		int argc = 0;
		int ignore_nl = tk->ignore_nl;
		int wait_colon = tk->wait_colon;
		tk->ignore_nl = TRUE;
		tk->wait_colon = FALSE;

		OpBuf_add_op2(buf, OP_FUNC, 0, vp_Value(func_strcat));

		if (tk->str_val.size > 0) {
			OpBuf_add_str(buf, tk);
			argc++;
		}
		Tok_next(tk);

		while (tk->v.type != TL_STRCAT_END) {
			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			argc++;

			if (tk->v.type == TL_FORMAT) {
				OpBuf_add_str(buf, tk);
				OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->str_tostr));
				Tok_next(tk);
			}

			if (tk->v.type != TL_STRCAT_MID) {
				break;
			}

			if (tk->str_val.size > 0) {
				OpBuf_add_str(buf, tk);
				argc++;
			}
			Tok_next(tk);
		}
		if (tk->v.type != TL_STRCAT_END) {
			unexpected_token_error(tk);
			return FALSE;
		}

		if (tk->str_val.size > 0) {
			OpBuf_add_str(buf, tk);
			argc++;
		}
		OpBuf_add_op2(buf, OP_CALL, argc, (Value)tk->v.line);

		tk->ignore_nl = ignore_nl;
		tk->wait_colon = wait_colon;
		Tok_next(tk);

		break;
	}
	case TL_VAR:
		if (Tok_peek(tk, 0) == T_ARROW){
			return parse_closure(buf, bk, tk);
		} else {
			RefNode *var;
			RefStr *name = Tok_intern(tk);

			var = Block_get_var(bk, name);
			if (var != NULL) {
				// ローカル変数
				OpBuf_add_op1(buf, (var->u.v.var ? OP_GET_LOCAL_V : OP_GET_LOCAL), var->u.v.offset);
			} else {
				RefNode *mod;

				if (tk->parse_cls != NULL) {
					var = Hash_get_p(&tk->m_var, name);
					if (var != NULL) {
						OpBuf_add_op2(buf, OP_GET_FIELD, var->u.v.offset, vp_Value(tk->parse_cls));
						Tok_next(tk);
						break;
					}
				}
				mod = Hash_get_p(bk->import, name);
				if (mod != NULL) {
					// モジュール名
					OpBuf_add_op2(buf, OP_MODULE, 0, vp_Value(mod));
				} else {
					// 未解決
					int pc = OpBuf_add_op2(buf, OP_FUNC, 0, VALUE_NULL);
					add_unresolved_top(tk, UNRESOLVED_IDENT, name, bk->func, pc);
				}
			}
			Tok_next(tk);
		}
		break;
	case TL_CLASS: {
		RefStr *name = Tok_intern(tk);
		int pc = OpBuf_add_op2(buf, OP_CLASS, 0, VALUE_NULL);
		add_unresolved_top(tk, UNRESOLVED_IDENT, name, bk->func, pc);
		Tok_next(tk);
		break;
	}
	case TL_CONST: {
		RefStr *name = Tok_intern(tk);
		int pc = OpBuf_add_op3(buf, OP_LITERAL_P, 0, (Value)tk->v.line, VALUE_NULL);
		add_unresolved_top(tk, UNRESOLVED_IDENT, name, bk->func, pc);
		Tok_next(tk);
		break;
	}
	case T_LP:
	case T_LP_C: {
		// (
		int peek1 = Tok_peek(tk, 0);
		if (peek1 == TL_VAR) {
			int peek2 = Tok_peek(tk, 1);
			if (peek2 == T_COLON || peek2 == T_COMMA || (peek2 == T_RP && Tok_peek(tk, 2) == T_ARROW)) {
				// (var:               (var,                 (var)=>
				return parse_closure(buf, bk, tk);
			}
		} else if (peek1 == T_RP) {
			return parse_closure(buf, bk, tk);
		}
		{
			// 括弧
			int ignore_nl = tk->ignore_nl;
			tk->ignore_nl = TRUE;

			Tok_next(tk);
			parse_expr(buf, bk, tk);

			if (tk->v.type != T_RP) {
				unexpected_token_error(tk);
				return FALSE;
			}
			tk->ignore_nl = ignore_nl;
			Tok_next(tk);
		}
		break;
	}
	case T_LB:
	case T_LB_C: {
		// 配列リテラル
		int argc = 0;

		int ignore_nl = tk->ignore_nl;
		tk->ignore_nl = TRUE;

		OpBuf_add_op2(buf, OP_FUNC, 0, vp_Value(func_array_new));
		Tok_next(tk);

		while (tk->v.type != T_RB) {
			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			argc++;

			if (tk->v.type != T_COMMA) {
				break;
			}
			Tok_next(tk);
		}
		if (tk->v.type != T_RB) {
			unexpected_token_error(tk);
			return FALSE;
		}
		tk->ignore_nl = ignore_nl;
		Tok_next(tk);
		OpBuf_add_op2(buf, OP_CALL, argc, (Value)tk->v.line);
		break;
	}
	case T_LC: {
		// 連想配列リテラル
		int argc = 0;

		int ignore_nl = tk->ignore_nl;
		tk->ignore_nl = TRUE;

		OpBuf_add_op2(buf, OP_FUNC, 0, vp_Value(func_map_new));
		Tok_next(tk);
		while (tk->v.type != T_RC) {
			if ((tk->v.type == TL_VAR || tk->v.type == TL_CLASS || tk->v.type == TL_CONST) && Tok_peek(tk, 0) == T_LET) {
				OpBuf_add_str(buf, tk);
				Tok_next(tk);
				Tok_next(tk);
			} else {
				if (!parse_expr(buf, bk, tk)) {
					return FALSE;
				}
				if (tk->v.type != T_COLON) {
					unexpected_token_error(tk);
					return FALSE;
				}
				Tok_next(tk);
			}
			argc++;

			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			argc++;
			if (tk->v.type != T_COMMA) {
				break;
			}
			Tok_next(tk);
		}
		if (tk->v.type != T_RC) {
			unexpected_token_error(tk);
			return FALSE;
		}
		tk->ignore_nl = ignore_nl;
		Tok_next(tk);
		OpBuf_add_op2(buf, OP_CALL, argc, (Value)tk->v.line);
		break;
	}
	default:
		unexpected_token_error(tk);
		return FALSE;
	}
	return TRUE;
}

static int parse_post_op(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A() , A[] , A.memb */
	if (!parse_elem(buf, bk, tk)) {
		return FALSE;
	}

	for (;;) {
		switch (tk->v.type) {
		case T_LP:
		case T_LB: {
			int type = tk->v.type;
			int tk_term = (type == T_LP ? T_RP : T_RB);
			int argc = 0;
			Value vp = VALUE_NULL;
			OpCode *p = OpBuf_prev(buf);

			int ignore_nl = tk->ignore_nl;
			tk->ignore_nl = TRUE;

			if (type == T_LP) {
				if (p->type == OP_GET_PROP) {
					vp = p->op[1];
					buf->cur = buf->prev;
					buf->prev = -1;
				} else if (p->type == OP_CLASS) {
					p->type = OP_FUNC;
					add_unresolved_memb(tk, bk->func, fs->str_new, buf->prev);
				}
			}
			Tok_next(tk);

			while (tk->v.type != tk_term) {
				if (!parse_expr(buf, bk, tk)) {
					return FALSE;
				}
				argc++;

				if (tk->v.type != T_COMMA) {
					break;
				}
				Tok_next(tk);
			}
			if (tk->v.type != tk_term) {
				unexpected_token_error(tk);
				return FALSE;
			}
			if (type == T_LP) {
				if (vp != VALUE_NULL) {
					OpBuf_add_op3(buf, OP_CALL_M, argc, (Value)tk->v.line, vp);
				} else {
					OpBuf_add_op2(buf, OP_CALL, argc, (Value)tk->v.line);
				}
			} else {
				OpBuf_add_op3(buf, OP_CALL_M, argc, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_LB]));
			}
			tk->ignore_nl = ignore_nl;
			Tok_next(tk);

			break;
		}
		case T_MEMB: {
			RefStr *name;

			Tok_next(tk);
			if (tk->v.type != TL_CLASS && tk->v.type != TL_VAR && tk->v.type != TL_CONST) {
				unexpected_token_error(tk);
				return FALSE;
			}
			name = intern(tk->str_val.p, tk->str_val.size);
			if (buf->cur > 0) {
				OpCode *op = OpBuf_prev(buf);
				switch (op->type) {
				case OP_MODULE:
					// 直前のOPコードがModuleの場合
					switch (tk->v.type) {
					case TL_CLASS:
						op->type = OP_CLASS;
						break;
					case TL_VAR:
						op->type = OP_FUNC;
						break;
					case TL_CONST:
						op->type = OP_LITERAL_P;
						// OP_CLASSの命令長は2,OP_LITERALの命令長は3
						OpBuf_grow(buf, 1);
						break;
					}
					add_unresolved_module(tk, UNRESOLVED_IDENT, name, Value_vp(op->op[0]), bk->func, buf->prev);
					break;
				case OP_CLASS:
					// 直前のOPコードがClassの場合
					switch (tk->v.type) {
					case TL_CLASS: {
						const RefNode *type = Value_vp(op->op[0]);
						define_error(tk, "%n has no member named %r", type, name);
						return FALSE;
					}
					case TL_VAR:
						op->type = OP_FUNC;
						break;
					case TL_CONST:
						op->type = OP_LITERAL_P;
						// OP_CLASSの命令長は2,OP_LITERALの命令長は3
						OpBuf_grow(buf, 1);
						break;
					}
					add_unresolved_memb(tk, bk->func, name, buf->prev);
					break;
				default:
					if (op->type != OP_GET_LOCAL || op->s != 0) {
						// 直前のOPがthisでない場合
						if (name->c[0] == '_') {
							define_error(tk, "%r is private", name);
							return FALSE;
						}
					}
					OpBuf_add_op3(buf, OP_GET_PROP, 0, (Value)tk->v.line, vp_Value(name));
					break;
				}
			} else {
				OpBuf_add_op3(buf, OP_GET_PROP, 0, (Value)tk->v.line, vp_Value(name));
			}
			Tok_next(tk);

			break;
		}
		default:
			goto BREAK;
		}
	}
BREAK:
	return TRUE;
}
static int parse_pre_op(OpBuf *buf, Block *bk, Tok *tk)
{
	/* +A , -A , !A , ~A */
	if (tk->v.type == T_ADD || tk->v.type == T_SUB || tk->v.type == T_INV || tk->v.type == T_NOT) {
		int type = tk->v.type;

		Tok_next(tk);
		if (!parse_pre_op(buf, bk, tk)) {
			return FALSE;
		}

		switch (type) {
		case T_ADD:
			OpBuf_add_op3(buf, OP_CALL_M, 0, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_PLUS]));
			break;
		case T_SUB:
			OpBuf_add_op3(buf, OP_CALL_M, 0, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_MINUS]));
			break;
		case T_INV:
			OpBuf_add_op3(buf, OP_CALL_M, 0, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_INV]));
			break;
		case T_NOT:
			OpBuf_add_op1(buf, OP_NOT, 0);
			break;
		}
	} else {
		if (!parse_post_op(buf, bk, tk)) {
			return FALSE;
		}
	}
	return TRUE;
}
static int parse_mul(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A * B , A / B , A % B */
	if (!parse_pre_op(buf, bk, tk)) {
		return FALSE;
	}

	while (tk->v.type == T_MUL || tk->v.type == T_DIV || tk->v.type == T_MOD) {
		int type = tk->v.type;
		Tok_next(tk);
		if (!parse_pre_op(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[type]));
	}

	return TRUE;
}
static int parse_add(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A + B , A - B */
	if (!parse_mul(buf, bk, tk)) {
		return FALSE;
	}

	while (tk->v.type == T_ADD || tk->v.type == T_SUB) {
		int type = tk->v.type;
		Tok_next(tk);
		if (!parse_mul(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[type]));
	}

	return TRUE;
}
static int parse_and(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A & B */
	if (!parse_add(buf, bk, tk)) {
		return FALSE;
	}

	while (tk->v.type == T_AND) {
		Tok_next(tk);
		if (!parse_add(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_AND]));
	}

	return TRUE;
}
static int parse_xor(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A ^ B */
	if (!parse_and(buf, bk, tk)) {
		return FALSE;
	}

	while (tk->v.type == T_XOR) {
		Tok_next(tk);
		if (!parse_and(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_XOR]));
	}

	return TRUE;
}
static int parse_or(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A | B */
	if (!parse_xor(buf, bk, tk)) {
		return FALSE;
	}

	while (tk->v.type == T_OR) {
		Tok_next(tk);
		if (!parse_xor(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_OR]));
	}

	return TRUE;
}
static int parse_shift(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A << B , A >> B */
	if (!parse_or(buf, bk, tk)) {
		return FALSE;
	}

	while (tk->v.type == T_LSH || tk->v.type == T_RSH) {
		int type = tk->v.type;
		Tok_next(tk);
		if (!parse_shift(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[type]));
	}

	return TRUE;
}
static int parse_range(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A .. B */
	if (!parse_shift(buf, bk, tk)) {
		return FALSE;
	}

	if (tk->v.type == T_DOT2 || tk->v.type == T_DOT3) {
		int include_end = (tk->v.type == T_DOT3);
		Tok_next(tk);
		if (!parse_shift(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op2(buf, OP_RANGE_NEW, include_end, (Value)tk->v.line);
	}

	return TRUE;
}
static int parse_equal(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A == B , A != B, A in B */
	if (!parse_range(buf, bk, tk)) {
		return FALSE;
	}

	if (tk->v.type >= T_EQ && tk->v.type <= T_GE) {
		int type = tk->v.type;
		Tok_next(tk);
		if (!parse_range(buf, bk, tk)) {
			return FALSE;
		}

		switch (type) {
		case T_EQ:
		case T_NEQ:
			OpBuf_add_op2(buf, OP_EQUAL, type, (Value)tk->v.line);
			break;
		case T_CMP:
			OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[T_CMP]));
			break;
		case T_LT:
		case T_LE:
		case T_GT:
		case T_GE:
			OpBuf_add_op2(buf, OP_CMP, type, (Value)tk->v.line);
			break;
		}
	} else if (tk->v.type == TT_IN) {
		Tok_next(tk);
		if (!parse_range(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op2(buf, OP_CALL_IN, 0, (Value)tk->v.line);
	}

	return TRUE;
}
static int parse_logand(OpBuf *buf, Block *bk, Tok *tk)
{
	int label;
	/* A && B */
	if (!parse_equal(buf, bk, tk)) {
		return FALSE;
	}

	label = 0;
	while (tk->v.type == T_LAND) {
		Tok_next(tk);
		label = OpBuf_add_op2(buf, OP_IF_NP, 0, (Value)label);
		if (!parse_equal(buf, bk, tk)) {
			return FALSE;
		}
	}

	// 逆順にたどる
	while (label > 0) {
		int next = (int)buf->p[label].op[0];
		buf->p[label].op[0] = (Value)buf->cur;
		label = next;
	}

	return TRUE;
}
static int parse_logor(OpBuf *buf, Block *bk, Tok *tk)
{
	int label;
	/* A || B */
	if (!parse_logand(buf, bk, tk)) {
		return FALSE;
	}

	label = 0;
	while (tk->v.type == T_LOR) {
		Tok_next(tk);
		label = OpBuf_add_op2(buf, OP_IF_P, 0, (Value)label);
		if (!parse_logand(buf, bk, tk)) {
			return FALSE;
		}
	}

	// 逆順にたどる
	while (label > 0) {
		int next = (int)buf->p[label].op[0];
		buf->p[label].op[0] = (Value)buf->cur;
		label = next;
	}

	return TRUE;
}
static int parse_expr(OpBuf *buf, Block *bk, Tok *tk)
{
	/* A ? B : C */
	if (!parse_logor(buf, bk, tk)) {
		return FALSE;
	}

	if (tk->v.type == T_QUEST) {
		int label1, label2;
		Tok_next(tk);

		label1 = OpBuf_add_op2(buf, OP_JIF_N, 0, VALUE_NULL);
		tk->wait_colon++;
		if (!parse_expr(buf, bk, tk)) {
			return FALSE;
		}

		if (tk->v.type != T_COLON) {
			unexpected_token_error(tk);
			return FALSE;
		}
		tk->wait_colon--;
		Tok_next(tk);

		label2 = OpBuf_add_op2(buf, OP_JMP, 0, VALUE_NULL);
		buf->p[label1].op[0] = (Value)buf->cur;

		if (!parse_expr(buf, bk, tk)) {
			return FALSE;
		}

		buf->p[label2].op[0] = (Value)buf->cur;
	}

	return TRUE;
}
/*
static int parse_express(OpBuf *buf, Block *bk, Tok *tk)
{
	buf->n_stack = bk->offset;
	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}
	if (bk->func->u.f.max_stack < buf->n_stack) {
		bk->func->u.f.max_stack = buf->n_stack;
	}
	return TRUE;
}
*/

////////////////////////////////////////////////////////////////////////////////

static int parse_statements(OpBuf *buf, Block *bk, Tok *tk, RefNode *klass);

static int parse_super(OpBuf *buf, Block *bk, Tok *tk, RefNode *klass)
{
	RefStr *name = fs->str_new;
	int argc = 0;

	if (tk->v.type == T_MEMB) {
		Tok_next(tk);
		if (tk->v.type != TL_VAR) {
			unexpected_token_error(tk);
			return FALSE;
		}
		name = intern(tk->str_val.p, tk->str_val.size);
		Tok_next(tk);
	}
	if (tk->v.type != T_LP) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next(tk);

	OpBuf_add_op1(buf, OP_GET_LOCAL, 0);

	tk->parse_cls = NULL;
	while (tk->v.type != T_RP) {
		if (!parse_expr(buf, bk, tk)) {
			return FALSE;
		}
		argc++;

		if (tk->v.type != T_COMMA) {
			break;
		}
		Tok_next(tk);
	}
	if (tk->v.type != T_RP) {
		unexpected_token_error(tk);
		return FALSE;
	}
	tk->parse_cls = klass;

	OpBuf_add_op3(buf, OP_CALL_INIT, argc, (Value)tk->v.line, vp_Value(name));
	Tok_next(tk);

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}

static int parse_statements_block(OpBuf *buf, Block *bk, Tok *tk);

/*
 * 識別子の後ろに=をつけて返す
 */
static RefStr *ident_add_equal(Tok *tk)
{
	char *cp = (char*)tk->str_val.p;
	cp[tk->str_val.size] = '=';
	tk->str_val.size++;
	return intern(tk->str_val.p, tk->str_val.size);
}
static int parse_expr_statement(OpBuf *buf, Block *bk, Tok *tk)
{
	union {
		OpCode lop;
		int64_t padding[4]; // 未使用
	} u;

	// 左辺値
	if (!parse_post_op(buf, bk, tk)) {
		return FALSE;
	}

	switch (tk->v.type) {
	case T_LET: {
		RefStr *set_prop_name = NULL;
		// 代入文
		memcpy(&u, buf->p + buf->prev, (buf->cur - buf->prev) * sizeof(int64_t));
		if (u.lop.type == OP_GET_PROP) {
			set_prop_name = ident_add_equal(tk);
		}
		buf->cur = buf->prev;
		buf->prev = -1;

		Tok_next(tk);
		if (!parse_expr(buf, bk, tk)) {
			return FALSE;
		}

		switch (u.lop.type) {
		case OP_GET_LOCAL_V:
			OpBuf_add_op1(buf, OP_SET_LOCAL, u.lop.s);
			break;
		case OP_GET_PROP:
			OpBuf_add_op3(buf, OP_CALL_M_POP, 1, u.lop.op[0], vp_Value(set_prop_name));
			break;
		case OP_GET_FIELD:
			OpBuf_add_op2(buf, OP_SET_FIELD, u.lop.s, vp_Value(tk->parse_cls));
			break;
		case OP_CALL_M:
			// a[i] = val
			if (Value_vp(u.lop.op[1]) == fs->symbol_stock[T_LB]) {
				// a.op_index_let(i, val)
				OpBuf_add_op3(buf, OP_CALL_M_POP, u.lop.s + 1, u.lop.op[0], vp_Value(fs->symbol_stock[T_LET_B]));
			} else {
				unexpected_token_error(tk);
				return FALSE;
			}
			break;
		default:
			syntax_error(tk, "Invalid lvalue");
			return FALSE;
		}
		break;
	}
	case T_L_ADD: case T_L_SUB: case T_L_MUL: case T_L_DIV: case T_L_MOD:
	case T_L_LSH: case T_L_RSH: case T_L_AND: case T_L_OR: case T_L_XOR: {
		RefStr *set_prop_name = NULL;
		int type = tk->v.type - (T_L_ADD - T_ADD);

		memcpy(&u, buf->p + buf->prev, (buf->cur - buf->prev) * sizeof(int64_t));
		if (u.lop.type == OP_GET_PROP) {
			set_prop_name = ident_add_equal(tk);
		}
		Tok_next(tk);

		switch (u.lop.type) {
		case OP_GET_LOCAL_V:
			// i += 1
			// i = i + 1
			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[type]));
			OpBuf_add_op1(buf, OP_SET_LOCAL, u.lop.s);
			break;
		case OP_GET_PROP:
			// a.memb += 1
			// a.memb = a.memb + 1
			buf->cur = buf->prev;
			buf->prev = -1;

			OpBuf_add_op1(buf, OP_DUP, 1);
			OpBuf_add_op3(buf, OP_GET_PROP, 0, (Value)tk->v.line, u.lop.op[1]);
			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			OpBuf_add_op3(buf, OP_CALL_M, 1, u.lop.op[0], vp_Value(fs->symbol_stock[type]));
			OpBuf_add_op3(buf, OP_CALL_M_POP, 1, u.lop.op[0], vp_Value(set_prop_name));
			break;
		case OP_GET_FIELD:
			// i += 1
			// i = i + 1
			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			OpBuf_add_op3(buf, OP_CALL_M, 1, (Value)tk->v.line, vp_Value(fs->symbol_stock[type]));
			OpBuf_add_op2(buf, OP_SET_FIELD, u.lop.s, vp_Value(tk->parse_cls));
			break;
		case OP_CALL_M:
			if (Value_vp(u.lop.op[1]) == fs->symbol_stock[T_LB]) {
				// a[i] += 1
				// a[i] = a[i] + 1
				buf->cur = buf->prev;
				buf->prev = -1;

				OpBuf_add_op1(buf, OP_DUP, 2);
				OpBuf_add_op3(buf, OP_CALL_M, 1, u.lop.op[0], vp_Value(fs->symbol_stock[T_LB]));
				if (!parse_expr(buf, bk, tk)) {
					return FALSE;
				}
				OpBuf_add_op3(buf, OP_CALL_M, 1, u.lop.op[0], vp_Value(fs->symbol_stock[type]));
				OpBuf_add_op3(buf, OP_CALL_M_POP, 2, u.lop.op[0], vp_Value(fs->symbol_stock[T_LET_B]));
			} else {
				syntax_error(tk, "Invalid lvalue");
				return FALSE;
			}
			break;
		default:
			syntax_error(tk, "Invalid lvalue");
			return FALSE;
		}
		break;
	}
	case T_INC: case T_DEC: {
		RefStr *set_prop_name;
		int type = tk->v.type;

		memcpy(&u, buf->p + buf->prev, (buf->cur - buf->prev) * sizeof(int64_t));

		switch (u.lop.type) {
		case OP_GET_LOCAL_V:
			// i++
			// i = i.__inc()
			OpBuf_add_op3(buf, OP_CALL_M, 0, (Value)tk->v.line, vp_Value(fs->symbol_stock[type]));
			OpBuf_add_op1(buf, OP_SET_LOCAL, u.lop.s);
			Tok_next(tk);
			break;
		case OP_GET_PROP:
			// a.memb++
			// a.memb = a.memb.__inc()
			Tok_next(tk);
			buf->cur = buf->prev;
			buf->prev = -1;

			OpBuf_add_op1(buf, OP_DUP, 1);
			OpBuf_add_op3(buf, OP_GET_PROP, 0, u.lop.op[0], u.lop.op[1]);
			OpBuf_add_op3(buf, OP_CALL_M, 0, u.lop.op[0], vp_Value(fs->symbol_stock[type]));

			set_prop_name = ident_add_equal(tk);
			OpBuf_add_op3(buf, OP_CALL_M_POP, 1, u.lop.op[0], vp_Value(set_prop_name));
			break;
		case OP_GET_FIELD: {
			OpBuf_add_op3(buf, OP_CALL_M, 0, (Value)tk->v.line, vp_Value(fs->symbol_stock[type]));
			OpBuf_add_op2(buf, OP_SET_FIELD, u.lop.s, vp_Value(tk->parse_cls));
			Tok_next(tk);
			break;
		}
		case OP_CALL_M:
			Tok_next(tk);
			if (Value_vp(u.lop.op[1]) == fs->symbol_stock[T_LB]) {
				// a[i]++
				// a[i] = a[i].__inc()
				buf->cur = buf->prev;
				buf->prev = -1;

				OpBuf_add_op1(buf, OP_DUP, 2);
				OpBuf_add_op3(buf, OP_CALL_M, 1, u.lop.op[0], vp_Value(fs->symbol_stock[T_LB]));
				OpBuf_add_op3(buf, OP_CALL_M, 0, u.lop.op[0], vp_Value(fs->symbol_stock[type]));
				OpBuf_add_op3(buf, OP_CALL_M_POP, 2, u.lop.op[0], vp_Value(fs->symbol_stock[T_LET_B]));
			} else {
				// a.hoge()++
				syntax_error(tk, "Invalid lvalue");
				return FALSE;
			}
			break;
		default:
			syntax_error(tk, "Invalid lvalue");
			return FALSE;
		}
		break;
	}
	case T_NL:
	case T_SEMICL:
	case T_RC:
	case T_EOF: {
		// 呼び出し文
		OpCode *p = buf->p + buf->prev;
		if (p->type == OP_CALL_M) {
			//Str *name = p->vp;
			p->type = OP_CALL_M_POP;
		} else if (p->type == OP_CALL) {
			p->type = OP_CALL_POP;
		} else {
			unexpected_token_error(tk);
			return FALSE;
		}
		break;
	}
	default: {
		// 括弧なしの呼び出し
		int argc = 0;
		Value vp = VALUE_NULL;
		OpCode *p = OpBuf_prev(buf);

		if (p->type == OP_GET_PROP) {
			// 戻る
			vp = p->op[1];
			buf->cur = buf->prev;
			buf->prev = -1;
		} else if (p->type == OP_CLASS) {
			// Class を Class.new に置き換え
			p->type = OP_FUNC;
			add_unresolved_memb(tk, bk->func, fs->str_new, buf->prev);
		}

		while (tk->v.type != T_NL && tk->v.type != T_SEMICL) {
			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			argc++;

			if (tk->v.type != T_COMMA) {
				break;
			}
			Tok_next(tk);
		}
		if (vp != VALUE_NULL) {
			OpBuf_add_op3(buf, OP_CALL_M_POP, argc, (Value)tk->v.line, vp);
		} else {
			OpBuf_add_op2(buf, OP_CALL_POP, argc, (Value)tk->v.line);
		}
		break;
	}
	}

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_RC || tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}

static int parse_local_var(OpBuf *buf, Block *bk, Tok *tk, int is_var)
{
	RefStr *name;
	RefNode *var;

	if (tk->v.type != TL_VAR) {
		unexpected_token_error(tk);
		return FALSE;
	}
	name = intern(tk->str_val.p, tk->str_val.size);
	if (tk->parse_cls != NULL) {
		var = Hash_get_p(&tk->m_var, name);
		if (var != NULL) {
			already_defined_name_error(tk, var);
			return FALSE;
		}
	}

	Tok_next(tk);
	if (tk->v.type != T_LET) {
		unexpected_token_error(tk);
		return FALSE;
	}

	Tok_next(tk);
	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}

	// 初期化式が完了してから識別子を追加
	var = Block_add_var(bk, tk, name, 0);
	if (var == NULL) {
		return FALSE;
	}
	var->u.v.var = is_var;

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}
static int parse_return(OpBuf *buf, Block *bk, Tok *tk)
{
	if (tk->v.type == T_NL || tk->v.type == T_SEMICL || tk->v.type == T_RC || tk->v.type == T_EOF) {
		OpBuf_add_op1(buf, OP_RETURN, 0);
	} else {
		if (!parse_expr(buf, bk, tk)) {
			return FALSE;
		}
		OpBuf_add_op1(buf, OP_RETURN_VAL, 0);
	}

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}
static int parse_yield(OpBuf *buf, Block *bk, Tok *tk)
{
	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}
	OpBuf_add_op1(buf, OP_YIELD_VAL, 0);

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}
static int parse_throw(OpBuf *buf, Block *bk, Tok *tk)
{
	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}
	OpBuf_add_op2(buf, OP_THROW, 0, (Value)tk->v.line);

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}

static int parse_block_statement(OpBuf *buf, Block *bk, Tok *tk)
{
	// スコープを作成
	Block *b = Block_new(bk, NULL);

	if (!parse_statements(buf, b, tk, NULL)) {
		return FALSE;
	}

	if (tk->v.type != T_RC) {
		unexpected_token_error(tk);
		return FALSE;
	}

	// スタック変数の除去
	if (b->h.count > 0) {
		OpBuf_add_op2(buf, OP_POP, b->h.count, (Value)tk->v.line);
	}
	Tok_next_skip(tk);

	return TRUE;
}
static int parse_if(OpBuf *buf, Block *bk, Tok *tk)
{
	int jmp, label;
	RefStr *c_name = NULL;

	// if let m = s.match(/a/) ...
	if (tk->v.type == TT_LET) {
		Tok_next(tk);
		if (tk->v.type != TL_VAR) {
			unexpected_token_error(tk);
			return FALSE;
		}
		c_name = intern(tk->str_val.p, tk->str_val.size);
		Tok_next(tk);
		if (tk->v.type != T_LET) {
			unexpected_token_error(tk);
			return FALSE;
		}
		Tok_next(tk);
	}
	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}

	Tok_skip(tk);
	label = 0;
	for (;;) {
		// スコープを作成
		Block *b = Block_new(bk, NULL);
		if (c_name != NULL) {
			// 変数宣言を行う
			if (!Block_add_var(b, tk, c_name, NODEOPT_READONLY)) {
				return FALSE;
			}
			jmp = OpBuf_add_op2(buf, OP_IFP_N, 0, VALUE_NULL);
		} else {
			jmp = OpBuf_add_op2(buf, OP_JIF_N, 0, VALUE_NULL);
		}
		if (tk->v.type != T_LC) {
			unexpected_token_error(tk);
			return FALSE;
		}
		Tok_next_skip(tk);
		if (!parse_statements(buf, b, tk, NULL)) {
			return FALSE;
		}
		if (tk->v.type != T_RC) {
			unexpected_token_error(tk);
			return FALSE;
		}
		if (b->h.count > 0) {
			OpBuf_add_op2(buf, OP_POP, b->h.count, (Value)tk->v.line);
		}
		Tok_next_skip(tk);

		if (tk->v.type == TT_ELSE) {
			label = OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);
			buf->p[jmp].op[0] = (Value)buf->cur;
			Tok_next_skip(tk);
			if (!parse_statements_block(buf, bk, tk)) {
				return FALSE;
			}
			break;
		} else if (tk->v.type == TT_ELIF) {
			label = OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);
			buf->p[jmp].op[0] = (Value)buf->cur;

			Tok_next(tk);
			if (tk->v.type == TT_LET) {
				Tok_next(tk);
				if (tk->v.type != TL_VAR) {
					unexpected_token_error(tk);
					return FALSE;
				}
				c_name = intern(tk->str_val.p, tk->str_val.size);
				Tok_next(tk);
				if (tk->v.type != T_LET) {
					unexpected_token_error(tk);
					return FALSE;
				}
				Tok_next(tk);
			} else {
				c_name = NULL;
			}
			if (!parse_expr(buf, bk, tk)) {
				return FALSE;
			}
			Tok_skip(tk);
		} else {
			// 終端
			buf->p[jmp].op[0] = (Value)buf->cur;
			break;
		}
	}

	// 逆順にたどる
	while (label > 0) {
		int next = (int)buf->p[label].op[0];
		buf->p[label].op[0] = (Value)buf->cur;
		label = next;
	}

	return TRUE;
}
static int parse_for(OpBuf *buf, Block *bk, Tok *tk)
{
	int label;
	int jmp;
	int iter;
	RefStr *c_name;
	Block *b;

	if (tk->v.type != TL_VAR) {
		unexpected_token_error(tk);
		return FALSE;
	}
	c_name = intern(tk->str_val.p, tk->str_val.size);

	Tok_next_skip(tk);
	if (tk->v.type != TT_IN) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next_skip(tk);

	// var iter = obj.iterator()
	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}
	OpBuf_add_op2(buf, OP_CALL_ITER, 0, (Value)tk->v.line);
	iter = bk->offset;
	bk->offset++;

	// スコープを作成
	b = Block_new(bk, NULL);

	label = buf->cur;
	bk->continue_p = label;

	// c = iter.next()
	OpBuf_add_op1(buf, OP_GET_LOCAL, iter);
	jmp = OpBuf_add_op3(buf, OP_CALL_NEXT, 0, (Value)tk->v.line, VALUE_NULL);
	if (!Block_add_var(b, tk, c_name, NODEOPT_READONLY)) {
		return FALSE;
	}

	Tok_skip(tk);
	if (tk->v.type != T_LC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next_skip(tk);
	if (!parse_statements(buf, b, tk, NULL)) {
		return FALSE;
	}
	if (tk->v.type != T_RC) {
		unexpected_token_error(tk);
		return FALSE;
	}

	OpBuf_add_op2(buf, OP_POP, b->h.count, (Value)tk->v.line);
	OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);
	buf->p[jmp].op[1] = (Value)buf->cur;
	Tok_next_skip(tk);

	// breakのラベルを書き換え
	bk->continue_p = -1;
	while (bk->break_p >= 0) {
		OpCode *p = &buf->p[bk->break_p];
		bk->break_p = (int)p->op[0];
		p->op[0] = (Value)buf->cur;
	}

	// iterator変数を削除
	OpBuf_add_op2(buf, OP_POP, 1, (Value)tk->v.line);
	bk->offset--;

	return TRUE;
}
static int parse_while(OpBuf *buf, Block *bk, Tok *tk)
{
	int label = buf->cur;
	int jmp;
	RefStr *c_name = NULL;
	Block *b;

	// while let m = s.match(/a/) ...
	if (tk->v.type == TT_LET) {
		Tok_next(tk);
		if (tk->v.type != TL_VAR) {
			unexpected_token_error(tk);
			return FALSE;
		}
		c_name = intern(tk->str_val.p, tk->str_val.size);
		Tok_next(tk);
		if (tk->v.type != T_LET) {
			unexpected_token_error(tk);
			return FALSE;
		}
		Tok_next(tk);
	}

	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}

	// スコープを作成
	b = Block_new(bk, NULL);
	if (c_name != NULL) {
		// 変数宣言を行う
		if (!Block_add_var(b, tk, c_name, NODEOPT_READONLY)) {
			return FALSE;
		}
		jmp = OpBuf_add_op2(buf, OP_IFP_N, 0, VALUE_NULL);
	} else {
		jmp = OpBuf_add_op2(buf, OP_JIF_N, 0, VALUE_NULL);
	}

	bk->continue_p = label;

	Tok_skip(tk);
	if (tk->v.type != T_LC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next_skip(tk);
	if (!parse_statements(buf, b, tk, NULL)) {
		return FALSE;
	}
	if (tk->v.type != T_RC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	if (b->h.count > 0) {
		OpBuf_add_op2(buf, OP_POP, b->h.count, (Value)tk->v.line);
	}

	OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);
	buf->p[jmp].op[0] = (Value)buf->cur;
	Tok_next_skip(tk);

	// breakのラベルを書き換え
	bk->continue_p = -1;
	while (bk->break_p >= 0) {
		OpCode *p = &buf->p[bk->break_p];
		bk->break_p = (int)p->op[0];
		p->op[0] = (Value)buf->cur;
	}

	return TRUE;
}
static int parse_switch(OpBuf *buf, Block *bk, Tok *tk)
{
	static Value switch_fault_message = VALUE_NULL;

	int jmp_next, jmp_else, label;
	int tmp_offset;
	int first;
	int expr_line = tk->v.line;

	if (!parse_expr(buf, bk, tk)) {
		return FALSE;
	}
	// スタックに残す
	tmp_offset = bk->offset;
	bk->offset++;

	Tok_skip(tk);
	if (tk->v.type != T_LC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next_skip(tk);

	jmp_next = -1;
	jmp_else = -1;
	label = 0;
	first = TRUE;
	for (;;) {
		if (tk->v.type == T_RC) {
			// switch終了
			Tok_next_skip(tk);
			break;
		} else if (tk->v.type == TT_CASE || tk->v.type == TT_DEFAULT) {
			if (!first) {
				label = OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);
			} else {
				first = FALSE;
			}
			// 直前の条件式で失敗したときのジャンプ先
			if (jmp_next > -1) {
				buf->p[jmp_next].op[0] = (Value)buf->cur;
			}

			if (tk->v.type == TT_DEFAULT) {
				// default節
				if (jmp_else > -1) {
					syntax_error(tk, "Multiple 'default' in switch");
					return FALSE;
				}
				Tok_next(tk);
				if (tk->v.type != T_NL && tk->v.type != T_SEMICL && tk->v.type != T_RC) {
					unexpected_token_error(tk);
					return FALSE;
				}

				// 常に条件不成立とみなす
				jmp_next = OpBuf_add_op2(buf, OP_JMP, 0, VALUE_NULL);
				jmp_else = buf->cur;
			} else {
				int jmp_expr = -1;
				Tok_next(tk);

				for (;;) {
					// 式の値とtmpの値を op_eq で比較
					if (!parse_expr(buf, bk, tk)) {
						return FALSE;
					}

					OpBuf_add_op1(buf, OP_GET_LOCAL, tmp_offset);
					OpBuf_add_op2(buf, OP_EQUAL2, T_EQ, (Value)tk->v.line);

					// コンマで連結した場合は、or と同じ
					if (tk->v.type == T_COMMA) {
						Tok_next_skip(tk);
						jmp_expr = OpBuf_add_op2(buf, OP_JIF, 0, (Value)jmp_expr);
					} else if (tk->v.type == T_NL || tk->v.type == T_SEMICL || tk->v.type == T_RC) {
						jmp_next = OpBuf_add_op2(buf, OP_JIF_N, 0, VALUE_NULL);
						break;
					} else {
						unexpected_token_error(tk);
						return FALSE;
					}
				}

				// 逆順にたどる
				while (jmp_expr > -1) {
					int next = (int)buf->p[jmp_expr].op[0];
					buf->p[jmp_expr].op[0] = (Value)buf->cur;
					jmp_expr = next;
				}
			}
			// スコープを作成
			{
				Block *b = Block_new(bk, NULL);
				if (!parse_statements(buf, b, tk, NULL)) {
					return FALSE;
				}
				if (b->h.count > 0) {
					OpBuf_add_op2(buf, OP_POP, b->h.count, (Value)tk->v.line);
				}
			}
		} else {
			unexpected_token_error(tk);
			return FALSE;
		}
	}

	if (jmp_else == -1) {
		// defaultがない場合、
		// default
		// throw SwitchError()
		// を挿入

		if (!first) {
			label = OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);
		}
		if (jmp_next > -1) {
			buf->p[jmp_next].op[0] = (Value)buf->cur;
		}
		jmp_next = OpBuf_add_op2(buf, OP_JMP, 0, VALUE_NULL);
		jmp_else = buf->cur;
		OpBuf_add_op2(buf, OP_CLASS, 0, vp_Value(fs->cls_switcherror));
		if (switch_fault_message == VALUE_NULL) {
			switch_fault_message = cstr_Value(fs->cls_str, "None of the case expressions match", -1);
		} else {
			Value_inc(switch_fault_message);
		}
		OpBuf_add_op2(buf, OP_LITERAL, 0, switch_fault_message);
		OpBuf_add_op2(buf, OP_CALL, 1, (Value)expr_line);
		OpBuf_add_op2(buf, OP_THROW, 0, (Value)expr_line);
	}
	// 最後のjmp先をdefaultにする
	buf->p[jmp_next].op[0] = (Value)jmp_else;

	// 逆順にたどる
	while (label > 0) {
		int next = (int)buf->p[label].op[0];
		buf->p[label].op[0] = (Value)buf->cur;
		label = next;
	}

	// 一時変数を削除
	OpBuf_add_op2(buf, OP_POP, 1, (Value)tk->v.line);
	bk->offset--;

	return TRUE;
}
static int parse_try(OpBuf *buf, Block *bk, Tok *tk)
{
	int jmp = 0, jmp2 = -1;
	int label = 0;

	// 本体
	{
		Block *b = Block_new(bk, NULL);
		jmp = OpBuf_add_op3(buf, OP_PUSH_CATCH, 0, VALUE_NULL, VALUE_NULL);
		b->offset++;

		if (tk->v.type != T_LC) {
			unexpected_token_error(tk);
			return FALSE;
		}
		Tok_next_skip(tk);
		if (!parse_statements(buf, b, tk, NULL)) {
			return FALSE;
		}
		if (tk->v.type != T_RC) {
			unexpected_token_error(tk);
			return FALSE;
		}

		// catchハンドラもpopの対象とする
		OpBuf_add_op2(buf, OP_POP, b->h.count + 1, (Value)tk->v.line);
		label = OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);
		Tok_next_skip(tk);
	}

	// catch-finally節
	for (;;) {
		if (tk->v.type == TT_CATCH) {
			RefNode *resolve_module;
			Block *b = Block_new(bk, NULL);

			// catch e:Error
			// catch e:module.Error
			Tok_next(tk);
			if (tk->v.type != TL_VAR) {
				unexpected_token_error(tk);
				return FALSE;
			}
			b->offset++;
			if (!Block_add_var(b, tk, intern(tk->str_val.p, tk->str_val.size), NODEOPT_READONLY)) {
				return FALSE;
			}

			Tok_next(tk);
			if (tk->v.type != T_COLON) {
				unexpected_token_error(tk);
				return FALSE;
			}

			Tok_next(tk);
			if (tk->v.type == TL_VAR) {
				// モジュール名が付く場合
				Str m_name = tk->str_val;
				resolve_module = Hash_get_p(bk->import, intern(m_name.p, m_name.size));
				if (resolve_module == NULL) {
					module_not_exist_error(tk, m_name);
					return FALSE;
				}
				Tok_next(tk);
				if (tk->v.type != T_MEMB) {
					unexpected_token_error(tk);
					return FALSE;
				}
				Tok_next(tk);
			} else {
				resolve_module = NULL;
			}
			if (tk->v.type != TL_CLASS) {
				unexpected_token_error(tk);
				return FALSE;
			}

			if (jmp2 >= 0) {
				// finally
				buf->p[jmp2].op[0] = (Value)buf->cur;
			} else {
				// catch
				buf->p[jmp].op[0] = (Value)buf->cur;
			}
			jmp2 = OpBuf_add_op3(buf, OP_CATCH_JMP, 0, VALUE_NULL, VALUE_NULL);
			if (resolve_module != NULL) {
				add_unresolved_module(tk, UNRESOLVED_CATCH, Tok_intern(tk), resolve_module, bk->func, jmp2);
			} else {
				add_unresolved_top(tk, UNRESOLVED_CATCH, Tok_intern(tk), bk->func, jmp2);
			}

			Tok_next(tk);
			if (tk->v.type != T_LC) {
				unexpected_token_error(tk);
				return FALSE;
			}
			Tok_next(tk);
			if (!parse_statements(buf, b, tk, NULL)) {
				return FALSE;
			}
			if (tk->v.type != T_RC) {
				unexpected_token_error(tk);
				return FALSE;
			}

			// e(例外)も含めてPOP
			OpBuf_add_op2(buf, OP_POP, b->h.count + 1, (Value)tk->v.line);
			label = OpBuf_add_op2(buf, OP_JMP, 0, (Value)label);

			Tok_next_skip(tk);
		} else {
			break;
		}
	}

	// 逆順にたどってジャンプ先を書き換える
	while (label > 0) {
		int next = (int)buf->p[label].op[0];
		buf->p[label].op[0] = (Value)buf->cur;
		label = next;
	}

	return TRUE;
}
static int parse_break(OpBuf *buf, Block *bk, Tok *tk)
{
	int pop_count = 0;
	if (tk->v.type != T_NL && tk->v.type != T_SEMICL && tk->v.type != T_RC && tk->v.type != T_EOF) {
		unexpected_token_error(tk);
		return FALSE;
	}

	for (;;) {
		if (bk->continue_p >= 0) {
			if (pop_count > 0) {
				OpBuf_add_op2(buf, OP_POP, pop_count, (Value)tk->v.line);
			}
			// 後でラベルを書き換える
			bk->break_p = OpBuf_add_op2(buf, OP_JMP, 0, (Value)bk->break_p);
			Tok_next(tk);
			break;
		}
		if (bk->parent == NULL) {
			syntax_error(tk, "break without loop");
			return FALSE;
		}
		// 除去するスタックの数を数える
		pop_count += (bk->offset - bk->parent->offset);
		bk = bk->parent;
	}
	return TRUE;
}
static int parse_continue(OpBuf *buf, Block *bk, Tok *tk)
{
	int pop_count = 0;

	if (tk->v.type != T_NL && tk->v.type != T_SEMICL && tk->v.type != T_RC && tk->v.type != T_EOF) {
		unexpected_token_error(tk);
		return FALSE;
	}

	for (;;) {
		if (bk->continue_p >= 0) {
			if (pop_count > 0) {
				OpBuf_add_op2(buf, OP_POP, pop_count, (Value)tk->v.line);
			}
			OpBuf_add_op2(buf, OP_JMP, 0, (Value)bk->continue_p);
			Tok_next(tk);
			break;
		}
		if (bk->parent == NULL) {
			syntax_error(tk, "continue without loop");
			return FALSE;
		}
		// 除去するスタックの数を数える
		pop_count += (bk->offset - bk->parent->offset);
		bk = bk->parent;
	}
	return TRUE;
}
static int parse_statements(OpBuf *buf, Block *bk, Tok *tk, RefNode *klass)
{
	if (klass != NULL) {
		Tok_skip(tk);
		OpBuf_add_op2(buf, OP_NEW_REF, 0, vp_Value(klass));
		if (tk->v.type == TT_SUPER) {
			Tok_next(tk);
			if (!parse_super(buf, bk, tk, klass)) {
				return FALSE;
			}
		} else {
			// 暗黙のsuper()呼び出し
			OpBuf_add_op1(buf, OP_GET_LOCAL, 0);  // push this
			OpBuf_add_op3(buf, OP_CALL_INIT, 0, (Value)tk->v.line, vp_Value(fs->str_new));
		}
	}

	for (;;) {
		switch (tk->v.type) {
		case TT_IF:
			Tok_next(tk);
			if (!parse_if(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_FOR:
			Tok_next(tk);
			if (!parse_for(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_WHILE:
			Tok_next(tk);
			if (!parse_while(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_SWITCH:
			Tok_next(tk);
			if (!parse_switch(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_TRY:
			Tok_next_skip(tk);
			if (!parse_try(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_LET:
			Tok_next(tk);
			if (!parse_local_var(buf, bk, tk, FALSE)) {
				return FALSE;
			}
			break;
		case TT_VAR:
			Tok_next(tk);
			if (!parse_local_var(buf, bk, tk, TRUE)) {
				return FALSE;
			}
			break;
		case TT_RETURN:
			if (tk->parse_genr) {
				// ジェネレーターでreturnすると、iterationを中止する
				// 値を返すことはできない
				OpBuf_add_op2(buf, OP_CLASS, 0, vp_Value(fs->cls_stopiter));
				OpBuf_add_op2(buf, OP_CALL, 0, (Value)tk->v.line);
				OpBuf_add_op2(buf, OP_THROW, 0, (Value)tk->v.line);
				Tok_next(tk);
				if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
					Tok_next(tk);
				} else if (tk->v.type != T_EOF) {
					unexpected_token_error(tk);
					return FALSE;
				}
			} else {
				Tok_next(tk);
				if (!parse_return(buf, bk, tk)) {
					return FALSE;
				}
			}
			break;
		case TT_YIELD:
			if (!tk->parse_genr) {
				unexpected_token_error(tk);
				return FALSE;
			}
			Tok_next(tk);
			if (!parse_yield(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_THROW:
			Tok_next(tk);
			if (!parse_throw(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_BREAK:
			Tok_next(tk);
			if (!parse_break(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_CONTINUE:
			Tok_next(tk);
			if (!parse_continue(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TL_INT:
		case TL_MPINT:
		case TL_FLOAT:
		case TL_STR:
		case TL_REGEX:
		case TL_CLASS:
		case TL_VAR:
		case TL_CONST:
		case TT_THIS:
		case T_LP:
		case T_LP_C:
		case T_LB:
			if (!parse_expr_statement(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case T_LC:
			Tok_next_skip(tk);
			if (!parse_block_statement(buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_SUPER:
			syntax_error(tk, "'super' cannot use here");
			return FALSE;
		case T_NL:
		case T_SEMICL:
			Tok_next(tk);
			break;
		default:
			// 終端
			goto BREAK;
		}
	}
BREAK:
	return TRUE;
}
static int parse_statements_block(OpBuf *buf, Block *bk, Tok *tk)
{
	Block *b = Block_new(bk, NULL);

	if (tk->v.type != T_LC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next_skip(tk);
	if (!parse_statements(buf, b, tk, NULL)) {
		return FALSE;
	}
	if (tk->v.type != T_RC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	if (b->h.count > 0) {
		OpBuf_add_op2(buf, OP_POP, b->h.count, (Value)tk->v.line);
	}
	Tok_next_skip(tk);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

static int parse_args(RefNode *node, Tok *tk, Block *bk)
{
	int abbr = -1;
	int num = 0;
	int type_check = FALSE;

	Tok_next(tk);
	for (;;) {
		RefStr *name;
		RefNode *var;

		if (tk->v.type != TL_VAR) {
			node->u.f.arg_min = (abbr == -1 ? num : abbr);
			node->u.f.arg_max = num;
			break;
		}

		name = Tok_intern(tk);
		if (tk->parse_cls != NULL) {
			var = Hash_get_p(&tk->m_var, name);
			if (var != NULL) {
				already_defined_name_error(tk, var);
				return FALSE;
			}
		}
		var = Block_add_var(bk, tk, name, NODEOPT_READONLY);
		if (var == NULL) {
			return FALSE;
		}

		Tok_next(tk);

		if (tk->v.type == T_COLON) {
			// 引数の型指定
			RefNode *resolve_module;

			Tok_next(tk);
			if (tk->v.type == TL_VAR) {
				// モジュール名が付く場合
				resolve_module = Hash_get_p(bk->import, Tok_intern(tk));
				if (resolve_module == NULL) {
					module_not_exist_error(tk, tk->str_val);
					return FALSE;
				}
				Tok_next(tk);
				if (tk->v.type != T_MEMB) {
					unexpected_token_error(tk);
					return FALSE;
				}
				Tok_next(tk);
			} else {
				resolve_module = NULL;
			}
			if (tk->v.type != TL_CLASS) {
				unexpected_token_error(tk);
				return FALSE;
			}
			if (resolve_module != NULL) {
				add_unresolved_module(tk, UNRESOLVED_ARGUMENT, Tok_intern(tk), resolve_module, node, num);
			} else {
				add_unresolved_top(tk, UNRESOLVED_ARGUMENT, Tok_intern(tk), node, num);
			}
			type_check = TRUE;
			Tok_next(tk);
		} else if (tk->v.type == T_DOT2) {
			// def fn(a, b, c..)
			if (abbr != -1) {
				// a = nullとa..を併用できない
				syntax_error(tk, "Cannot use 'arg = null' and 'arg..'");
				return FALSE;
			}
			Tok_next_skip(tk);

			node->u.f.arg_min = num;
			node->u.f.arg_max = -1;
			break;
		}

		if (tk->v.type == T_LET) {
			// 引数省略可能
			Tok_next(tk);
			if (tk->v.type != TT_NULL) {
				unexpected_token_error(tk);
				return FALSE;
			}
			Tok_next(tk);

			if (abbr == -1) {
				abbr = num;
			}
		} else {
			if (abbr != -1) {
				syntax_error(tk, "'= null' arguments must be right side");
				return FALSE;
			}
		}
		num++;
		if (num > 127) {
			syntax_error(tk, "Too many arguments (> 127)");
			return FALSE;
		}

		Tok_skip(tk);
		if (tk->v.type != T_COMMA) {
			node->u.f.arg_min = (abbr == -1 ? num : abbr);
			node->u.f.arg_max = num;
			break;
		}
		Tok_next_skip(tk);
	}

	if (type_check) {
		node->u.f.arg_type = Mem_get(&fg->st_mem, sizeof(RefNode*) * num);
		memset(node->u.f.arg_type, 0, sizeof(RefNode*) * num);
	} else {
		node->u.f.arg_type = NULL;
	}

	if (tk->v.type != T_RP) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next_skip(tk);

	return TRUE;
}
static int parse_body(RefNode *node, Tok *tk, RefNode *construct, Block *bk)
{
	OpBuf buf;
	OpBuf_init(&buf);

	Tok_next(tk);

	if (tk->parse_genr) {
		OpBuf_add_op1(&buf, OP_INIT_GENR, 0);
	}
	if (!parse_statements(&buf, bk, tk, construct)) {
		return FALSE;
	}

	if (bk->h.count > 0) {
		OpBuf_add_op2(&buf, OP_POP, bk->h.count, (Value)tk->v.line);
	}
	if (tk->v.type != T_RC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next(tk);

	if (construct != NULL) {
		OpBuf_add_op1(&buf, OP_RETURN_THRU, 0);
	} else if (tk->parse_genr) {
		OpBuf_add_op2(&buf, OP_CLASS, 0, vp_Value(fs->cls_stopiter));
		OpBuf_add_op2(&buf, OP_CALL, 0, (Value)tk->v.line);
		OpBuf_add_op2(&buf, OP_THROW, 0, (Value)tk->v.line);
	} else {
		OpBuf_add_op1(&buf, OP_RETURN, 0);
	}
	node->u.f.u.op = OpBuf_fix(&buf, &fg->st_mem);
	node->u.f.max_stack = buf.n_stack;

#ifdef DUMP_OPCODE
	test_expr(&buf);
#endif

	OpBuf_close(&buf);
	return TRUE;
}
static int parse_body_ep(RefNode *node, Tok *tk, Block *bk)
{
	OpBuf buf;
	int ret;
	OpBuf_init(&buf);

	ret = parse_expr(&buf, bk, tk);
	OpBuf_add_op1(&buf, OP_RETURN_VAL, 0);
	OpBuf_add_op1(&buf, OP_NONE, 0);
	node->u.f.u.op = OpBuf_fix(&buf, &fg->st_mem);
	node->u.f.max_stack = buf.n_stack;

#ifdef DUMP_OPCODE
	test_expr(&buf);
#endif

	OpBuf_close(&buf);
	return ret;
}
static int parse_function(RefNode *node, Tok *tk, RefNode *construct)
{
	Block *bk = Block_new(NULL, node);

	if (tk->v.type == T_LP) {
		if (!parse_args(node, tk, bk)) {
			return FALSE;
		}
	} else if (construct == NULL) {
		node->opt = NODEOPT_PROPERTY;
		node->u.f.arg_min = 0;
		node->u.f.arg_max = 0;
		node->u.f.arg_type = NULL;
	} else {
		unexpected_token_error(tk);
		return FALSE;
	}

	if (node->name == fs->str_dispose) {
		if (node->u.f.arg_max > 0) {
			syntax_error(tk, "'_dispose' not allowed any arguments");
			return FALSE;
		}
	}

	if (tk->v.type == T_ARROW) {
		if (construct) {
			unexpected_token_error(tk);
			return FALSE;
		}
		// def fn(x) => x * 2
		Tok_next(tk);
		if (!parse_body_ep(node, tk, bk)) {
			return FALSE;
		}
	} else {
		Tok_skip(tk);
		if (tk->v.type == T_LC) {
			// def fn(x){ return x*2 }
			if (!parse_body(node, tk, construct, bk)) {
				return FALSE;
			}
			return TRUE;
		}
		unexpected_token_error(tk);
		return FALSE;
	}

	return TRUE;
}
static int parse_const(RefNode *node, Tok *tk)
{
	RefStr *name;
	RefNode *nd;
	Block bk;

	if (tk->v.type != TL_CONST) {
		unexpected_token_error(tk);
		return FALSE;
	}
	name = Tok_intern(tk);
	nd = Node_define_tk(node, tk, name, NODE_CONST_U);
	if (nd == NULL) {
		return FALSE;
	}

	Tok_next(tk);
	if (tk->v.type != T_LET) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next(tk);

	bk.parent = NULL;
	bk.break_p = -1;
	bk.continue_p = -1;

	bk.offset = 1;
	bk.func = nd;
	bk.import = &nd->defined_module->u.m.import;
	bk.h.count = 0;

	memset(&nd->u.f, 0, sizeof(nd->u.f));

	if (!parse_body_ep(nd, tk, &bk)) {
		return FALSE;
	}

	if (tk->v.type == T_NL || tk->v.type == T_SEMICL) {
		Tok_next(tk);
		return TRUE;
	} else if (tk->v.type == T_EOF) {
		return TRUE;
	}
	unexpected_token_error(tk);
	return FALSE;
}

// node : Module || Class
static int parse_def_statement(RefNode *node, Tok *tk)
{
	int genr = FALSE;
	if (tk->v.type == T_MUL) {
		Tok_next(tk);
		genr = TRUE;
	}

	if (tk->v.type == TL_VAR) {
		RefStr *name = Tok_intern(tk);
		RefNode *nd = Node_define_tk(node, tk, name, NODE_FUNC);
		if (nd == NULL) {
			return FALSE;
		}
		nd->u.f.klass = NULL;

		Tok_next(tk);
		if (tk->v.type != T_LP) {
			unexpected_token_error(tk);
			return FALSE;
		}
		tk->parse_genr = genr;
		if (!parse_function(nd, tk, NULL)) {
			return FALSE;
		}
		tk->parse_genr = FALSE;
	} else if (tk->v.type == TL_CONST) {
		if (genr) {
			unexpected_token_error(tk);
			return FALSE;
		}
		return parse_const(node, tk);
	} else {
		unexpected_token_error(tk);
		return FALSE;
	}
	return TRUE;
}

static void define_default_eq(RefNode *klass, NativeFunc func)
{
	Hash *h = &klass->u.c.h;
	RefNode *m = klass->defined_module;
	RefNode *n;

	if (Hash_get_p(h, fs->symbol_stock[T_EQ]) == NULL) {
		n = define_identifier_p(m, klass, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
		define_native_func_a(n, func, 1, 1, NULL, klass);
	}
}
static void define_default_hash(RefNode *klass, RefNode *func)
{
	HashEntry *he = Hash_get_add_entry(&klass->u.c.h, &fg->st_mem, fs->str_hash);
	he->p = func;
}
// baseクラスのメンバ関数をklassにコピーする
// 重複した場合継承先を優先する
// ただし、_disposeは継承しない
void extends_method(RefNode *klass, RefNode *base)
{
	Hash *src = &base->u.c.h;
	Hash *dst = &klass->u.c.h;
	int n = src->entry_num;
	int i;

	klass->u.c.super = base;

	for (i = 0; i < n; i++) {
		HashEntry *p;

		for (p = src->entry[i]; p != NULL; p = p->next) {
			RefNode *node = p->p;

			if ((node->type == NODE_FUNC || node->type == NODE_FUNC_N) && node->name != fs->str_dispose) {
				HashEntry *he = Hash_get_add_entry(dst, &fg->st_mem, p->key);
				if (he->p == NULL) {
					he->p = node;
				}
			}
		}
	}

	if ((klass->opt & NODEOPT_INTEGRAL) != 0) {
		// integral互換の場合、op_eqを設定
		if (klass != fs->cls_int) {
			define_default_eq(klass, object_eq);
		}
	} else if ((klass->opt & NODEOPT_STRCLASS) != 0) {
		// str互換の場合、hash, op_eqを設定
		define_default_eq(klass, sequence_eq);
		define_default_hash(klass, refnode_sequence_hash);
	}
}
static int parse_class_statement(RefNode *module, Tok *tk, int abst)
{
	RefStr *name;
	RefNode *klass;
	int n_memb = 0;

	if (tk->v.type != TL_CLASS) {
		unexpected_token_error(tk);
		return FALSE;
	}
	// クラス名
	name = Tok_intern(tk);

	klass = Node_define_tk(module, tk, name, NODE_CLASS);
	if (klass == NULL) {
		return FALSE;
	}
	if (abst) {
		klass->opt = NODEOPT_ABSTRACT;
	}

	Tok_next(tk);

	if (tk->v.type == T_COLON) {
		// クラス継承
		RefNode *resolve_module;
		Tok_next(tk);

		klass->type = NODE_CLASS_U;
		Tok_next(tk);
		if (tk->v.type == TL_VAR) {
			// モジュール名が付く場合
			Str m_name = tk->str_val;
			resolve_module = Hash_get(&module->u.m.import, m_name.p, m_name.size);
			if (resolve_module == NULL) {
				module_not_exist_error(tk, m_name);
				return FALSE;
			}
			Tok_next(tk);
			if (tk->v.type != T_MEMB) {
				unexpected_token_error(tk);
				return FALSE;
			}
			Tok_next(tk);

			add_unresolved_module(tk, UNRESOLVED_EXTENDS, Tok_intern(tk), resolve_module, klass, 0);
		} else {
			add_unresolved_top(tk, UNRESOLVED_EXTENDS, Tok_intern(tk), klass, 0);
		}
	}

	Tok_skip(tk);
	if (tk->v.type != T_LC) {
		unexpected_token_error(tk);
		return FALSE;
	}
	Tok_next(tk);

	tk->parse_cls = klass;
	Hash_init(&tk->m_var, &fv->cmp_mem, 16);

	for (;;) {
		switch (tk->v.type) {
		case TL_VAR: {
			// new constructor(...)
			RefNode *nd;

			name = Tok_intern(tk);
			if (name != fs->str_new) {
				unexpected_token_error(tk);
				return FALSE;
			}
			Tok_next(tk);
			if (tk->v.type == TL_VAR) {
				// 名前つきコンストラクタ
				name = Tok_intern(tk);
				if (name == fs->str_new) {
					unexpected_token_error(tk);
					return FALSE;
				}
				Tok_next(tk);
			}
			nd = Node_define_tk(klass, tk, name, NODE_NEW);
			if (nd == NULL) {
				return FALSE;
			}
			nd->u.f.klass = klass;
			if (!parse_function(nd, tk, klass)) {
				return FALSE;
			}
			break;
		}
		case TT_DEF: {
			int genr = FALSE;

			Tok_next(tk);
			if (tk->v.type == T_MUL) {
				Tok_next(tk);
				genr = TRUE;
			}
			if (tk->v.type == TL_CONST) {
				if (genr) {
					unexpected_token_error(tk);
					return FALSE;
				}
				parse_const(klass, tk);
			} else if (tk->v.type == TL_VAR) {
				RefNode *nd;
				Str s = tk->str_val;

				Tok_next(tk);
				if (tk->v.type == T_LET) {
					char *cp = (char*)s.p;
					if (genr) {
						unexpected_token_error(tk);
						return FALSE;
					}
					cp[s.size] = '=';
					s.size++;
					Tok_next(tk);
				}
				name = intern(s.p, s.size);
				nd = Node_define_tk(klass, tk, name, NODE_FUNC);
				if (nd == NULL) {
					return FALSE;
				}
				nd->u.f.klass = klass;

				tk->parse_genr = genr;
				if (!parse_function(nd, tk, NULL)) {
					return FALSE;
				}
				tk->parse_genr = FALSE;
			} else {
				unexpected_token_error(tk);
				return FALSE;
			}
			break;
		}
		case TT_VAR: {
			HashEntry *he;
			RefNode *var;

			Tok_next(tk);
			if (tk->v.type != TL_VAR) {
				unexpected_token_error(tk);
				return FALSE;
			}
			name = Tok_intern(tk);
			he = Hash_get_add_entry(&tk->m_var, &fv->cmp_mem, name);
			if (he->p != NULL) {
				already_defined_name_error(tk, he->p);
				return FALSE;
			}
			var = Mem_get(&fv->cmp_mem, sizeof(RefNode));
			var->uid = NULL;
			var->type = NODE_VAR;
			var->opt = 0;
			var->name = name;
			var->defined_module = tk->module;
			var->defined_line = tk->v.line;

			var->u.v.offset = n_memb;
			n_memb++;
			he->p = var;
			Tok_next(tk);

			break;
		}
		case T_SEMICL:
		case T_NL:
			Tok_next(tk);
			break;
		case T_RC:
			Tok_next(tk);
			tk->parse_cls = NULL;
			if (klass->type == NODE_CLASS) {
				extends_method(klass, fs->cls_obj);
			}
			klass->u.c.n_memb = n_memb;
			return TRUE;
		default:
			unexpected_token_error(tk);
			return FALSE;
		}
	}
}
RefNode *init_toplevel(RefNode *module)
{
	// トップレベルに書いた文は最初に実行される
	RefNode *nd = Node_define(module, module, fs->str_toplevel, NODE_FUNC, NULL);
	nd->u.f.arg_min = 0;
	nd->u.f.arg_max = 0;
	nd->u.f.arg_type = NULL;
	nd->u.f.klass = NULL;
	return nd;
}

void add_unresolved_args(RefNode *cur_mod, RefNode *mod, const char *name, RefNode *func, int argn)
{
	UnresolvedID *uid = Node_add_unresolve(mod, cur_mod, 0, intern(name, -1));
	Unresolved *p = Mem_get(&fv->cmp_mem, sizeof(Unresolved));

	p->type = UNRESOLVED_ARGUMENT;
	p->u.node = func;
	p->pc = argn;

	// 末尾に追加
	p->next = uid->ur;
	uid->ur = p;
}
void add_unresolved_ptr(RefNode *cur_mod, RefNode *mod, const char *name, RefNode **ptr)
{
	UnresolvedID *uid = Node_add_unresolve(mod, cur_mod, 0, intern(name, -1));
	Unresolved *p = Mem_get(&fv->cmp_mem, sizeof(Unresolved));

	p->type = UNRESOLVED_POINTER;
	p->u.pnode = ptr;

	// 末尾に追加
	p->next = uid->ur;
	uid->ur = p;
}

static int get_module_from_native(RefNode **pmod, const char *filename)
{
	void *handle;
	FoxModuleDefine define_module;
	const char *err_str = NULL;
	RefNode *module = new_Module(TRUE);

	handle = dlopen_fox(filename, RTLD_LAZY);
	if (handle == NULL) {
		throw_errorf(fs->mod_lang, "InternalError", "%s", dlerror_fox());
		return FALSE;
	}
	dlerror_fox();  // clear error

	define_module = dlsym_fox(handle, "define_module");
	err_str = dlerror_fox();

	if (err_str != NULL || define_module == NULL) {
		throw_errorf(fs->mod_lang, "InternalError", "%s", dlerror_fox());
		return FALSE;
	}
	define_module(module, fs, fg);
	*pmod = module;

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// ptr      p           end
// KEYSTRING=VALUESTRING
static void set_env_value(const char *ptr, const char *p, const char *end)
{
	RefStr *key = intern(ptr, p - ptr);
	char *val = str_dup_p(p + 1, (end - p) - 1, &fg->st_mem);
	Hash_add_p(&fs->envs, &fg->st_mem, key, val);
}

// ptr      p            end
// KEYSTRING+=VALUESTRING
static void append_env_value(const char *ptr, const char *p, const char *end)
{
	RefStr *key = intern(ptr, p - ptr);
	Str val_append = Str_new(p + 2, end - p - 2);
	HashEntry *entry = Hash_get_add_entry(&fs->envs, &fg->st_mem, key);
	const char *value = entry->p;

	Str val = Str_new(value, -1);
	if (val.size == 0 || val.p[val.size - 1] == PATH_SPLIT_C) {
		char *newval = Mem_get(&fg->st_mem, val.size + val_append.size + 1);
		if (val.size > 0) {
			memcpy(newval, val.p, val.size);
		}
		memcpy(newval + val.size, val_append.p, val_append.size);
		newval[val.size  + val_append.size] = '\0';
		entry->p = newval;
	} else {
		char *newval = Mem_get(&fg->st_mem, val.size + val_append.size + 2);
		if (val.p != NULL && val.size > 0) {
			memcpy(newval, val.p, val.size);
		}
		newval[val.size] = PATH_SPLIT_C;
		memcpy(newval + val.size + 1, val_append.p, val_append.size);
		newval[val.size  + val_append.size + 1] = '\0';
		entry->p = newval;
	}
}

static int parse_path_env(PtrList **proot, const char *key)
{
	const char *src_p = Hash_get(&fs->envs, key, -1);
	int ret = FALSE;

	if (src_p != NULL) {
		char *p = str_dup_p(src_p, -1, &fg->st_mem);

		while (*p != '\0') {
			const char *top = p;
			while (*p != '\0' && *p != PATH_SPLIT_C) {
				p++;
			}

			if (p > top) {
				char *abs_path = path_normalize(NULL, fs->cur_dir, top, p - top, NULL);
				int len = strlen(abs_path);
				PtrList *pl = PtrList_add(proot, len + 1, &fg->st_mem);
				strcpy(pl->u.c, abs_path);
				free(abs_path);
				ret = TRUE;
			}
			if (*p != '\0') {
				p++;
			}
		}
	}
	return ret;
}
static int load_max_alloc(void)
{
	const char *p = Hash_get(&fs->envs, "FOX_MAX_ALLOC", -1);

	if (p != NULL) {
		int max_alloc = parse_memory_size(Str_new(p, -1));
		if (max_alloc > 0) {
			if (max_alloc < 1024 * 1024) {
				fs->max_alloc = 1024 * 1024;
			} else {
				fs->max_alloc = max_alloc;
			}
			return TRUE;
		}
	}
	return FALSE;
}
static int load_stdio_charset(void)
{
	const char *name = Hash_get(&fs->envs, "FOX_STDIO_CHARSET", -1);
	if (name != NULL) {
		RefCharset *cs = get_charset_from_name(name, -1);
		if (cs != NULL) {
			fs->cs_stdio = cs;
			return TRUE;
		}
	}
	return FALSE;
}
static int load_error_dst(void)
{
	const char *edst = Hash_get(&fs->envs, "FOX_ERROR", -1);
	if (edst != NULL) {
		fv->err_dst = edst;
		return TRUE;
	}
	return FALSE;
}
void load_env_settings(int *defs)
{
	if (parse_path_env(&import_path, "FOX_IMPORT") && defs != NULL) {
		defs[ENVSET_IMPORT] = TRUE;
	}
	if (parse_path_env(&resource_path, "FOX_RESOURCE") && defs != NULL) {
		defs[ENVSET_RESOURCE] = TRUE;
	}
	if (load_max_alloc() && defs != NULL) {
		defs[ENVSET_MAX_ALLOC] = TRUE;
	}
	if (load_stdio_charset() && defs != NULL) {
		defs[ENVSET_STDIO_CHARSET] = TRUE;
	}
	if (load_error_dst() && defs != NULL) {
		defs[ENVSET_ERROR] = TRUE;
	}
}
static void add_default_path(PtrList **proot, const char *key)
{
	char *path = str_printf("%S" SEP_S "%s", fs->fox_home, key);
	int len = strlen(path);
	PtrList *pl = PtrList_add(proot, len + 1, &fg->st_mem);
	strcpy(pl->u.c, path);
	free(path);
}

static int parse_pragma(RefNode *module, Tok *tk, Str line)
{
	Str key, val;
	const char *p = line.p;
	const char *end = p + line.size;

	key = line;
	val.p = NULL;
	val.size = 0;

	while (p < end) {
		if (module == fv->startup) {
			if (*p == '=') {
				// =が出たら環境変数の設定(上書き)
				set_env_value(line.p, p, end);
				return TRUE;
			} else if (p + 1 < end && p[0] == '+' && p[1] == '=') {
				// +=が出たら環境変数の追加
				append_env_value(line.p, p, end);
				return TRUE;
			}
		}
		// スペースで2つに分ける
		if (isspace_fox(*p)) {
			key = Str_new(line.p, p - line.p);
			while (p < end && isspace_fox(*p)) {
				p++;
			}
			val.p = p;
			val.size = end - p;
			break;
		}
		p++;
	}
	if (key.size == 0) {
		syntax_error(tk, "Pragma error");
		return FALSE;
	}

	switch (key.p[0]) {
	case 'c':
		if (Str_eq_p(key, "cgi")) {
			fs->cgi_mode = TRUE;
			return TRUE;
		}
		break;
	case 'f':
		if (Str_eq_p(key, "foxinfo")) {
			if (module == fv->startup) {
				if (fs->cgi_mode) {
					cgi_init_responce();
				}
				init_fox_stack();
				print_foxinfo();
				fox_close();
				exit(0);
			}
		}
		break;
	case 'n':
		if (Str_eq_p(key, "native")) {
			int result = FALSE;
			if (val.size >= 1) {
				switch (val.p[0]) {
				case 'm':
					if (Str_eq_p(val, "marshal")) {
						define_marshal_module(module);
						result = TRUE;
					}
					break;
				}
			}
			if (result) {
				return TRUE;
			} else {
				syntax_error(tk, "Unknown native %Q", val);
				return FALSE;
			}
		}
		break;
	default:
		break;
	}
	syntax_error(tk, "Unknown pragma %S", key);
	return FALSE;
}

/**
 * bk != NULL  top != NULL : 継続実行
 * bk == NULL, top != NULL : eval
 * bk == NULL, top == NULL : 静的コンパイル
 */
static int parse_source(RefNode *module, Tok *tk, RefNode *top, Block *bk)
{
	OpBuf buf;
	int is_cont;

	if (top == NULL) {
		top = init_toplevel(module);
		bk = Block_new(NULL, top);
		is_cont = FALSE;
	} else if (bk == NULL) {
		bk = Block_new(NULL, top);
		is_cont = FALSE;
	} else {
		top = bk->func;
		is_cont = TRUE;
	}
	OpBuf_init(&buf);

	// プラグマ
	tk->mode_pragma = FALSE;
	for (;;) {
		switch (tk->v.type) {
		case TL_PRAGMA: {
			Str line = tk->str_val;
			if (line.size > 0 && line.p[0] != '!') {
				if (!parse_pragma(module, tk, line)) {
					return FALSE;
				}
			}
			Tok_next(tk);
			break;
		}
		case T_NL:
			Tok_next(tk);
			break;
		default:
			goto BREAK1;
		}
	}
BREAK1:
	if (module == fv->startup) {
		load_env_settings(NULL);
		add_default_path(&resource_path, "res");
	}

	// import宣言
	for (;;) {
		switch (tk->v.type) {
		case TT_IMPORT:
			if (!parse_import(module, tk)) {
				return FALSE;
			}
			break;
		case T_NL:
		case T_SEMICL:
			Tok_next(tk);
			break;
		default:
			goto BREAK2;
		}
	}
BREAK2:
	tk->mode_pragma = FALSE;

	for (;;) {
		switch (tk->v.type) {
		case TT_ABSTRACT:
			Tok_next(tk);
			if (tk->v.type != TT_CLASS) {
				unexpected_token_error(tk);
				return FALSE;
			}
			Tok_next(tk);
			if (!parse_class_statement(module, tk, TRUE)) {
				return FALSE;
			}
			break;
		case TT_CLASS:
			Tok_next(tk);
			if (!parse_class_statement(module, tk, FALSE)) {
				return FALSE;
			}
			break;
		case TT_DEF:
			Tok_next(tk);
			if (!parse_def_statement(module, tk)) {
				return FALSE;
			}
			break;
		case TT_LET:
			Tok_next(tk);
			if (!parse_local_var(&buf, bk, tk, FALSE)) {
				return FALSE;
			}
			break;
		case TT_VAR:
			Tok_next(tk);
			if (!parse_local_var(&buf, bk, tk, TRUE)) {
				return FALSE;
			}
			break;
		case TT_IF:
			Tok_next(tk);
			if (!parse_if(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_FOR:
			Tok_next(tk);
			if (!parse_for(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_WHILE:
			Tok_next(tk);
			if (!parse_while(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_SWITCH:
			Tok_next(tk);
			if (!parse_switch(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_TRY:
			Tok_next_skip(tk);
			if (!parse_try(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_RETURN:
			Tok_next(tk);
			if (!parse_return(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_THROW:
			Tok_next(tk);
			if (!parse_throw(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_BREAK:
			Tok_next(tk);
			if (!parse_break(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_CONTINUE:
			Tok_next(tk);
			if (!parse_continue(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case TT_THIS:
			syntax_error(tk, "'this' cannot use here");
			return FALSE;
		case TT_SUPER:
			syntax_error(tk, "'super' cannot use here");
			return FALSE;
		case TL_INT:
		case TL_MPINT:
		case TL_FLOAT:
		case TL_STR:
		case TL_REGEX:
		case TL_CLASS:
		case TL_VAR:
		case TL_CONST:
		case T_LP:
		case T_LB:
			if (!parse_expr_statement(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case T_LC:
			Tok_next_skip(tk);
			if (!parse_block_statement(&buf, bk, tk)) {
				return FALSE;
			}
			break;
		case T_NL:
		case T_SEMICL:
			Tok_next(tk);
			break;
		case T_EOF: {
			if (is_cont) {
				OpBuf_add_op1(&buf, OP_SUSPEND, 0);
			} else {
				if (bk->h.count > 0) {
					OpBuf_add_op2(&buf, OP_POP, bk->h.count, (Value)tk->v.line);
				}
				OpBuf_add_op1(&buf, OP_RETURN, 0);
			}
			OpBuf_add_op1(&buf, OP_NONE, 0);

			top->u.f.u.op = OpBuf_fix(&buf, tk->st_mem);
			top->u.f.max_stack = buf.n_stack;

#ifdef DUMP_OPCODE
			test_expr(&buf);
#endif
			OpBuf_close(&buf);
			return TRUE;
		}
		default:
			unexpected_token_error(tk);
			return FALSE;
		}
	}
}

/**
 * raise_errorファイルが存在しないときエラーを発生
 */
static int load_htfox_sub(const char *path, Hash *hash, int raise_error)
{
	char *ptr;
	char *buf;

	HashEntry *he = Hash_get_add_entry(hash, &fv->cmp_mem, intern(path, -1));
	if (he->p != NULL) {
		// 同じファイルを2回includeした場合、無視する
		return TRUE;
	}
	he->p = (void*)1;

	buf = read_from_file(NULL, path, NULL);
	if (buf == NULL) {
		if (raise_error) {
			throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
			return FALSE;
		} else {
			return TRUE;
		}
	}
	ptr = buf;

	while (*ptr != '\0') {
		char *top = ptr;
		Str line;
		int i;

		while (*ptr != '\0' && *ptr != '\n') {
			ptr++;
		}

		line = Str_new(top, ptr - top);
		// \rで終わっていたら、末尾を削除
		if (line.size > 0 && line.p[line.size - 1] == '\r') {
			line.size--;
		}

		// 先頭が#で始まっていたらコメント
		if (line.size == 0 || *top == '#') {
			if (*ptr == '\n') {
				ptr++;
			}
			continue;
		}
		for (i = 0; i < line.size; i++) {
			char c = line.p[i];
			if (c == '=') {
				set_env_value(line.p, line.p + i, line.p + line.size);
				break;
			} else if (i < line.size && c == '+' && line.p[i + 1] == '=') {
				append_env_value(line.p, line.p + i, line.p + line.size);
				break;
			}
		}
		if (*ptr == '\n') {
			ptr++;
		}
	}
	return TRUE;
}
/**
 * 同じディレクトリにある.htfoxを読む
 */
int load_htfox()
{
	Hash hash;
	Hash_init(&hash, &fv->cmp_mem, 16);
	return load_htfox_sub(str_printf("%S.htfox", fs->cur_dir), &hash, FALSE);
}

/**
 * ソースコード文字列からModuleを生成
 * モジュール名は ""
 */
RefNode *get_module_from_src(const char *src_p, int src_size)
{
	Tok tk;
	char *srcfile = str_dup_p(src_p, src_size, &fg->st_mem);
	RefStr *name = intern("[argument]", 10);
	RefNode *module = new_Module(FALSE);
	module->name = name;

	if (fv->startup == NULL) {
		fv->startup = module;
	}

	Tok_init(&tk, module, srcfile);

	// よく使う名前空間を登録
	PtrList_add_p(&module->u.m.usng, fs->mod_lang, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_io, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_file, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_mime, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_locale, &fg->st_mem);

	// moduleプールに登録
	Hash_add_p(&fg->mod_root, &fg->st_mem, name, module);

	if (!parse_source(module, &tk, NULL, NULL)) {
	}
	module->u.m.loaded = TRUE;
	Tok_close(&tk);

	return module;
}

#ifdef DEBUGGER

int set_module_eval(RefNode *module, char *src_p, RefNode *func, Block *block)
{
	Tok tk;
	Tok_init(&tk, module, src_p);

	if (!parse_source(module, &tk, func, block)) {
		Tok_close(&tk);
		return FALSE;
	}
	Tok_close(&tk);
	return TRUE;
}

#endif

/**
 * モジュールが登録されていなければ動的に読み込み
 * 読み込みエラーなら例外をセットしてFALSEを返す
 * ファイルが見つからない場合はretにNULLをセットして、エラーとしない
 * ファイルが見つかればエラーでもmoduleを返す
 *
 * path_p : 正規化済みファイルパス
 */
static int module_load_sub(RefNode **ret, Str name, const char *path_p, int initialize, int native)
{
	if (exists_file(path_p) == EXISTS_FILE) {
		RefNode *module = NULL;
		int result = TRUE;

		if (initialize) {
			fox_init_compile(FALSE);
		}
		if (native) {
			if (!get_module_from_native(&module, path_p)) {
				result = FALSE;
			}
		} else {
			module = get_module_by_file(path_p);
		}
		if (module != NULL) {
			RefStr *name_k = intern(name.p, name.size);
			module->name = name_k;
			Hash_add_p(&fg->mod_root, &fg->st_mem, name_k, module);
		} else {
			if (!native) {
				throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path_p, -1));
			}
		}
		if (initialize) {
			if (fg->error != VALUE_NULL) {
				Mem_close(&fv->cmp_mem);
				result = FALSE;
			} else if (!fox_link()) {
				Mem_close(&fv->cmp_mem);
				result = FALSE;
			}
		}
		*ret = module;
		return result;
	}

	*ret = NULL;
	return TRUE;
}
static int exists_import_dir(Str name)
{
	return FALSE;
}
/**
 * パッケージ名からモジュールを取得
 * name_p     : パッケージ名
 * syslib     : $FOX_HOME/importのみから検索する
 * initialize : 名前解決を行う
 * rootに登録する
 * すでに登録されている場合は再利用する
 */
static int get_module_by_name_sub(RefNode **mod_p, Str name, const char *name_p, const char *ext_p, int initialize, int native)
{
	RefNode *mod;
	int ret;
	char *path = str_printf("%S" SEP_S "import" SEP_S "%s.%s", fs->fox_home, name_p, ext_p);
	ret = module_load_sub(mod_p, name, path, initialize, native);
	mod = *mod_p;

	if (mod != NULL) {
		char *ptr = Mem_get(&fg->st_mem, name.size + 10);
		char *p;
		sprintf(ptr, "[system]%s", name_p);
		for (p = ptr + 8; *p != '\0'; p++) {
			if (*p == SEP_C) {
				*p = '.';
			}
		}
		mod->u.m.src_path = ptr;
	}
	free(path);
	return ret;
}
RefNode *get_module_by_name(const char *name_ptr, int name_size, int syslib, int initialize)
{
	Str name = Str_new(name_ptr, name_size);
	char *name_p;
	int i;

	RefNode *mod = Hash_get(&fg->mod_root, name_ptr, name_size);
	if (mod != NULL) {
		return mod;
	}
	name_p = str_dup_p(name_ptr, name_size, NULL);

	for (i = 0; name_p[i] != '\0'; i++) {
		if (name_p[i] == '.') {
			name_p[i] = SEP_C;
		} else if (!isalnum(name_p[i]) && name_p[i] != '_') {
			goto FINALLY;
		}
	}

	if (!syslib) {
		PtrList *lst;
		for (lst = import_path; lst != NULL; lst = lst->next) {
			int ret;
			char *path = str_printf("%s" SEP_S "%s.fox", lst->u.c, name_p);
			ret = module_load_sub(&mod, name, path, initialize, FALSE);
			if (mod != NULL) {
				mod->u.m.src_path = str_dup_p(path, -1, &fg->st_mem);
				if (exists_import_dir(name)) {
					// 
				}
			}
			free(path);
			if (!ret) {
				free(name_p);
				return NULL;
			}
			if (mod != NULL) {
				break;
			}
		}
	}
	if (mod == NULL) {
		if (!get_module_by_name_sub(&mod, name, name_p, "fox", initialize, FALSE)) {
			free(name_p);
			return NULL;
		}
	}
	if (mod == NULL) {
		if (!get_module_by_name_sub(&mod, name, name_p, "so", initialize, TRUE)) {
			free(name_p);
			return NULL;
		}
	}

FINALLY:
	if (mod == NULL) {
		throw_errorf(fs->mod_lang, "ImportError", "Cannot find source file for %S", name);
	}
	free(name_p);
	return mod;
}

/*
 * path_p : 正規化済み
 * ファイルが見つからない場合、==NULL
 * ファイルが見つかった場合は、!=NULL
 * エラーはfg->errorで返す
 * rootに登録しない
 */
RefNode *get_module_by_file(const char *path_p)
{
	RefNode *module;
	Tok tk;

	char *srcfile = read_from_file(NULL, path_p, NULL);
	if (srcfile == NULL) {
		return NULL;
	}

	module = new_Module(FALSE);
	if (fv->startup == NULL) {
		fv->startup = module;
	}

	// よく使う名前空間を登録
	PtrList_add_p(&module->u.m.usng, fs->mod_lang, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_io, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_file, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_mime, &fg->st_mem);
	PtrList_add_p(&module->u.m.usng, fs->mod_locale, &fg->st_mem);

	Tok_init(&tk, module, srcfile);

	if (!parse_source(module, &tk, NULL, NULL)) {
	}
	module->u.m.loaded = TRUE;
	free(srcfile);
	Tok_close(&tk);

	return module;
}
RefNode *get_node_member(RefNode *node, ...)
{
	va_list va;
	va_start(va, node);

	for (;;) {
		RefNode *n;
		RefStr *rs = va_arg(va, RefStr*);

		if (rs == NULL) {
			break;
		}
		if (node->type != NODE_MODULE && node->type != NODE_CLASS) {
			throw_errorf(fs->mod_lang, "NameError", "%r is not defined.", rs);
			return NULL;
		}
		n = Hash_get_p(&node->u.c.h, rs);
		if (n == NULL) {
			throw_errorf(fs->mod_lang, "NameError", "%r is not defined.", rs);
			return NULL;
		}
		node = n;
	}

	va_end(va);
	return node;
}
