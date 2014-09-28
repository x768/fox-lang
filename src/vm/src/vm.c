#include "fox_parse.h"
#include "datetime.h"
#include <string.h>
#include <time.h>


typedef struct {
	RefNode *node;
	UnresolvedID *uid;
} NodeAndUnresolved;

static void ambiguous_reference(RefNode *module, int line, RefNode *n1, RefNode *n2)
{
	RefStr *src1 = n1->defined_module->name;
	RefStr *src2 = n2->defined_module->name;

	throw_errorf(fs->mod_lang, "DefineError", "Anbiguous reference %n (%r or %r)", n1, src1, src2);
	add_stack_trace(module, NULL, line);
}

#if 0
void put_log(const char *msg, int msg_size)
{
	FileHandle fd = open_fox("/home/frog/Public/logs/log.txt", O_CREAT|O_WRONLY|O_APPEND, DEFAULT_PERMISSION);
	if (fd != -1) {
		if (msg_size < 0) {
			msg_size = strlen(msg);
		}
		write_fox(fd, msg, msg_size);
		close_fox(fd);
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 共有ライブラリに渡す関数をセット
 */
void init_so_func(void)
{
	fs->str_dup_p = str_dup_p;
	fs->str_printf = str_printf;

	fs->StrBuf_init = StrBuf_init;
	fs->StrBuf_init_refstr = StrBuf_init_refstr;
	fs->StrBuf_str_Value = StrBuf_str_Value;
	fs->StrBuf_add = StrBuf_add;
	fs->StrBuf_add_c = StrBuf_add_c;
	fs->StrBuf_add_r = StrBuf_add_r;
	fs->StrBuf_alloc = StrBuf_alloc;
	fs->StrBuf_printf = StrBuf_printf;
	fs->intern = intern;
	fs->get_intern = get_intern;
	fs->Mem_init = Mem_init;
	fs->Mem_get = Mem_get;
	fs->Mem_close = Mem_close;
	fs->Mem_close = Mem_close;
	fs->Hash_init = Hash_init;
	fs->Hash_add_p = Hash_add_p;
	fs->Hash_get = Hash_get;
	fs->Hash_get_p = Hash_get_p;
	fs->Hash_get_add_entry = Hash_get_add_entry;

	fs->Value_type = Value_type;
	fs->Value_int = Value_int;
	fs->Value_float = Value_float;
	fs->Value_str = Value_str;
	fs->Value_frac_s = Value_frac_s;
	fs->Value_frac10 = Value_frac10;
	fs->Value_timestamp = Value_timestamp;

	fs->Value_cp = Value_cp;
	fs->ref_cp_Value = ref_cp_Value;
	fs->int64_Value = int64_Value;
	fs->float_Value = float_Value;
	fs->cstr_Value = cstr_Value;
	fs->cstr_Value_conv = cstr_Value_conv;
	fs->frac_s_Value = frac_s_Value;
	fs->frac10_Value = frac10_Value;
	fs->time_Value = time_Value;

	fs->new_ref = new_ref;
	fs->new_buf = new_buf;
	fs->new_refstr_n = new_refstr_n;

	fs->Value_inc = Value_inc;
	fs->Value_dec = Value_dec;
	fs->Value_push = Value_push;
	fs->Value_pop = Value_pop;
	fs->get_node_member = get_node_member;

	fs->is_subclass = is_subclass;
	fs->get_module_by_name = get_module_by_name;
	fs->define_identifier = define_identifier;
	fs->define_identifier_p = define_identifier_p;
	fs->define_native_func_a = define_native_func_a;
	fs->extends_method = extends_method;
	fs->add_unresolved_module_p = add_unresolved_module_p;
	fs->add_unresolved_args = add_unresolved_args;
	fs->add_unresolved_ptr = add_unresolved_ptr;

	fs->throw_errorf = throw_errorf;
	fs->throw_error_select = throw_error_select;
	fs->fatal_errorf = fatal_errorf;
	fs->throw_stopiter = throw_stopiter;
	fs->file_value_to_path = file_value_to_path;
	fs->value_to_streamio = value_to_streamio;
	fs->get_random = get_random;
	fs->fix_bigint = (void (*)(Value *v, void *mp))fix_bigint;
	fs->bytesio_get_strbuf = bytesio_get_strbuf;

	fs->get_loader_function = get_loader_function;
	fs->load_aliases_file = load_aliases_file;
	fs->resource_to_path = resource_to_path;
	fs->mimetype_from_name_refstr = mimetype_from_name_refstr;
	fs->mimetype_from_suffix = mimetype_from_suffix;
	fs->mimetype_from_magic = mimetype_from_magic;
	fs->utf8_next = utf8_next;
	fs->utf8_codepoint_at = utf8_codepoint_at;
	fs->get_charset_from_name = get_charset_from_name;

	fs->IconvIO_open = IconvIO_open;
	fs->IconvIO_close = IconvIO_close;
	fs->IconvIO_conv = IconvIO_conv;

	fs->call_function = call_function;
	fs->call_function_obj = call_function_obj;
	fs->call_member_func = call_member_func;
	fs->call_property = call_property;

	fs->init_stream_ref = init_stream_ref;
	fs->stream_read_data = stream_read_data;
	fs->stream_write_data = stream_write_data;
	fs->stream_seek_sub = stream_seek;
	fs->stream_flush_sub = stream_flush_sub;

	fs->get_local_tz = get_local_tz;
	fs->adjust_timezone = adjust_timezone;
	fs->adjust_date = adjust_date;
	fs->Calendar_to_Time = Calendar_to_Time;
	fs->timedelta_parse_string = timedelta_parse_string;

	fs->refarray_new = refarray_new;
	fs->refarray_push = refarray_push;
	fs->refmap_new = refmap_new;
	fs->refmap_add = refmap_add;
	fs->refmap_get = refmap_get;
	fs->refmap_get_strkey = refmap_get_strkey;
	fs->refmap_del = refmap_del;

	fs->get_file_size = get_file_size;
	fs->read_from_file = read_from_file;
}

/**
 * スタブを作成
 */
static void init_first_classes()
{
	fs->cls_str = Mem_get(&fg->st_mem, sizeof(RefNode));
	fs->cls_module = Mem_get(&fg->st_mem, sizeof(RefNode));
	fs->cls_class = Mem_get(&fg->st_mem, sizeof(RefNode));
	fs->cls_fn = Mem_get(&fg->st_mem, sizeof(RefNode));
	fs->cls_locale = Mem_get(&fg->st_mem, sizeof(RefNode));
	fs->cls_file = Mem_get(&fg->st_mem, sizeof(RefNode));
}
void init_fox_vm()
{
	RefNode *mod_time;

	fs = malloc(sizeof(FoxStatic));
	memset(fs, 0, sizeof(FoxStatic));

	fg = malloc(sizeof(FoxGlobal));
	memset(fg, 0, sizeof(FoxGlobal));
	Mem_init(&fg->st_mem, 16 * 1024);

	fv = malloc(sizeof(FoxVM));
	memset(fv, 0, sizeof(FoxVM));


	fs->max_alloc = 32 * 1024 * 1024;  // 一時的
	init_first_classes();
	g_intern_init();
	fs->fox_home = get_fox_home();

	fs->symbol_stock[T_EQ] = intern("op_eq", -1);
	fs->symbol_stock[T_CMP] = intern("op_cmp", -1);

	fs->symbol_stock[T_ADD] = intern("op_add", -1);
	fs->symbol_stock[T_SUB] = intern("op_sub", -1);
	fs->symbol_stock[T_MUL] = intern("op_mul", -1);
	fs->symbol_stock[T_DIV] = intern("op_div", -1);
	fs->symbol_stock[T_MOD] = intern("op_mod", -1);
	fs->symbol_stock[T_LSH] = intern("op_lsh", -1);
	fs->symbol_stock[T_RSH] = intern("op_rsh", -1);
	fs->symbol_stock[T_INV] = intern("op_inv", -1);
	fs->symbol_stock[T_AND] = intern("op_and", -1);
	fs->symbol_stock[T_OR] = intern("op_or", -1);
	fs->symbol_stock[T_XOR] = intern("op_xor", -1);
	fs->symbol_stock[T_PLUS] = intern("op_plus", -1);
	fs->symbol_stock[T_MINUS] = intern("op_minus", -1);

	fs->symbol_stock[T_LB] = intern("op_index", -1);
	fs->symbol_stock[T_LET_B] = intern("op_index=", -1);

	fs->symbol_stock[T_INC] = intern("op_inc", -1);
	fs->symbol_stock[T_DEC] = intern("op_dec", -1);
	fs->symbol_stock[T_IN] = intern("op_in", -1);
	fs->symbol_stock[T_LP_C] = intern("op_call", -1);

	fs->str_0 = intern(NULL, 0);
	fs->str_new = intern("new", -1);
	fs->str_dispose = intern("_dispose", -1);
	fs->str_tostr = intern("to_str", -1);
	fs->str_hash = intern("hash", -1);
	fs->str_iterator = intern("iterator", -1);
	fs->str_next = intern("next", -1);
	fs->str_read = intern("read", -1);
	fs->str_write = intern("write", -1);
	fs->str_marshal_read = intern("_marshal_read", -1);
	fs->str_marshal_write = intern("_marshal_write", -1);

	fs->str_missing = intern("_missing", -1);
	fs->str_missing_set = intern("_missing=", -1);
	fs->str_method_missing = intern("_method_missing", -1);
	fs->str_anonymous = intern("[anonymous]", -1);
	fs->str_toplevel = intern("[toplevel]", -1);

	fs->Str_EMPTY.p = NULL;
	fs->Str_EMPTY.size = 0;

	Hash_init(&fg->mod_root, &fg->st_mem, 32);

	fv->integral_num = 0;
	fv->integral_max = 64;
	fv->integral = malloc(sizeof(RefNode*) * fv->integral_max);

	init_lang_module_stubs();
	init_io_module_stubs();
	init_file_module_stubs();
	init_mime_module_stubs();
	mod_time = init_time_module_stubs();

	init_lang_module_1();
	init_cgi_module_1();
	init_locale_module_1();
	init_time_module_1(mod_time);
	init_io_module_1();
	init_file_module_1();
	init_mime_module_1();
	init_so_func();
}
void init_fox_stack()
{
	fg->stk = Mem_get(&fg->st_mem, sizeof(Value) * fs->max_stack);
	fg->stk_max = fg->stk + fs->max_stack - 16;   // StackOverflowの基準
	fg->stk_base = fg->stk;
	fg->stk_top = fg->stk + 1;
	fg->stk[0] = VALUE_NULL;
	fv->n_callfunc = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * まず、自分自身のmoduleから探す
 * なければ、importされているmoduleを探す
 * NULLを返したら即終了
 */
static RefNode *find_ident_top(RefNode *m, RefStr *name, UnresolvedID *uid)
{
	PtrList *lst;
	RefNode *n;

	// 自分のmodule優先
	n = Hash_get_p(&m->u.m.h, name);
	if (n != NULL) {
		return n;
	}

	if (name->c[0] == '_') {
		throw_error_select(THROW_NOT_DEFINED__REFSTR, name);
		add_stack_trace(m, NULL, uid->ref_line);
		return NULL;
	}

	for (lst = m->u.m.usng; lst != NULL; lst = lst->next) {
		RefNode *mod = lst->u.p;
		RefNode *n2 = Hash_get_p(&mod->u.m.h, name);
		if (n2 != NULL) {
			if (n == NULL) {
				n = n2;
			} else {
				// 2回以上出現した場合はエラー
				ambiguous_reference(m, uid->ref_line, n2, n);
				return NULL;
			}
		}
	}
	if (n == NULL) {
		throw_error_select(THROW_NOT_DEFINED__REFSTR, name);
		add_stack_trace(m, NULL, uid->ref_line);
	}
	return n;
}
/**
 * UnresolvedID単位で識別子を解決する
 */
static int resolve_main(UnresolvedID *uid, RefNode *node, PtrList **extends, PtrList **catches)
{
	Unresolved *ur;

	for (ur = uid->ur; ur != NULL; ur = ur->next) {
		RefNode *n = ur->u.node;

		switch (ur->type) {
		case UNRESOLVED_IDENT: {
			OpCode *op = n->u.f.u.op;

			switch (node->type) {
			case NODE_CONST:     // 計算済みの定数
			case NODE_CONST_U:   // 未計算の定数
			case NODE_CONST_U_N: // 未計算の定数
				op[ur->pc].op[1] = vp_Value(node);
				break;
			default: // func / class
				op[ur->pc].op[0] = vp_Value(node);
				break;
			}
			break;
		}
		case UNRESOLVED_CATCH: {
			NodeAndUnresolved *nu = Mem_get(&fv->cmp_mem, sizeof(NodeAndUnresolved));
			n->u.f.u.op[ur->pc].op[1] = vp_Value(node);
			nu->node = node;
			nu->uid = uid;
			PtrList_add_p(catches, nu, &fv->cmp_mem);
			break;
		}
		case UNRESOLVED_ARGUMENT: {
			RefNode **type = n->u.f.arg_type;
			type[ur->pc] = node;
			break;
		}
		case UNRESOLVED_POINTER:
			*ur->u.pnode = node;
			break;
		case UNRESOLVED_EXTENDS:
			n->u.c.super = node;
			PtrList_add_p(extends, n, &fv->cmp_mem);
			break;
		}
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * クラス継承を行う
 * 再帰的に呼び出される
 */
static int extends_class(RefNode *klass)
{
	RefNode *super = (RefNode*)klass->u.c.super;

	if (super->type == NODE_CLASS_P) {
		// 継承が循環している
		throw_errorf(fs->mod_lang, "DefineError", "Circular inheritance detected.");
		add_stack_trace(klass->defined_module, NULL, klass->defined_line);
		return FALSE;
	} else if (super->type == NODE_CLASS_U) {
		// 継承元クラスを先に解決
		if (!extends_class(super)) {
			return FALSE;
		}
	} else if (super->type != NODE_CLASS) {
		throw_errorf(fs->mod_lang, "TypeNameError", "%r must be a class", super->name);
		add_stack_trace(klass->defined_module, NULL, klass->defined_line);
		return FALSE;
	} else  if ((super->opt & NODEOPT_ABSTRACT) == 0) {
		throw_errorf(fs->mod_lang, "TypeNameError", "Abstract class required");
		add_stack_trace(klass->defined_module, NULL, klass->defined_line);
		return FALSE;
	}

	// 継承元のメンバを追加
	klass->u.c.n_offset = super->u.c.n_memb;
	klass->u.c.n_memb += super->u.c.n_memb;
	extends_method(klass, super);

	klass->type = NODE_CLASS;
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * コンパイル開始前に呼び出す
 */
void fox_init_compile(int dynamic)
{
	UnresolvedMemb *ur;

	Mem_init(&fv->cmp_mem, 32 * 1024);
	fg->error = VALUE_NULL;

	ur = Mem_get(&fv->cmp_mem, sizeof(UnresolvedMemb));
	ur->next = NULL;
	fv->unresolved_memb_root = ur;
	fv->unresolved_memb = ur;
	fv->unresolved_memb_n = 0;

	fv->cmp_dynamic = dynamic;
}

/**
 * 未解決の識別子を解決して一時メモリを解放する
 */
int fox_link()
{
	int i;
	PtrList *extends = NULL;
	PtrList *catches = NULL;
	UnresolvedMemb *urm;

	for (i = 0; i < fg->mod_root.entry_num; i++) {
		HashEntry *entry;

		for (entry = fg->mod_root.entry[i]; entry != NULL; entry = entry->next) {
			RefNode *m = entry->p;
			int j;

			// importから検索する
			for (j = 0; j < m->u.m.unresolved.entry_num; j++) {
				HashEntry *entry2;

				for (entry2 = m->u.m.unresolved.entry[j]; entry2 != NULL; entry2 = entry2->next) {
					RefStr *name = entry2->key;
					UnresolvedID *uid = entry2->p;
					RefNode *node = find_ident_top(m, name, uid);
					if (node == NULL) {
						return FALSE;
					}
					resolve_main(uid, node, &extends, &catches);
				}
			}
			// 終わったのでunresolvedを削除 (二度と使われない)
			m->u.m.unresolved.entry_num = 0;

			// 名前空間を明示してある識別子
			for (j = 0; j < m->u.m.h.entry_num; j++) {
				HashEntry *entry2;

				for (entry2 = m->u.m.h.entry[j]; entry2 != NULL; entry2 = entry2->next) {
					RefStr *name = entry2->key;
					RefNode *node = entry2->p;
					UnresolvedID *uid = node->uid;

					if (uid != NULL) {
						if (node->type == NODE_UNRESOLVED) {
							throw_error_select(THROW_NOT_DEFINED__REFSTR, name);
							add_stack_trace(uid->ref_module, NULL, uid->ref_line);
							return FALSE;
						}
						resolve_main(uid, node, &extends, &catches);

						// 終わったら削除 (再度作成される可能性がある)
						node->uid = NULL;
					}
				}
			}
		}
	}
	// クラス継承の処理
	while (extends != NULL) {
		RefNode *klass = extends->u.p;
		if (klass->type == NODE_CLASS_U) {
			if (!extends_class(extends->u.p)) {
				return FALSE;
			}
		}
		extends = extends->next;
	}
	// catchの型がErrorを継承しているか
	while (catches != NULL) {
		NodeAndUnresolved *nu = catches->u.p;
		if (!is_subclass(nu->node, fs->cls_error)) {
			throw_errorf(fs->mod_lang, "TypeNameError", "Exceptions must extends from 'Error'");
			add_stack_trace(nu->uid->ref_module, NULL, nu->uid->ref_line);
			return FALSE;
		}
		catches = catches->next;
	}

	// Class.membの解決
	for (urm = fv->unresolved_memb_root; urm != NULL; urm = urm->next) {
		int n;
		if (urm->next != NULL) {
			n = MAX_UNRSLV_NUM;
		} else {
			n = fv->unresolved_memb_n;
		}
		for (i = 0; i < n; i++) {
			UnrslvMemb *um = &urm->rslv[i];
			RefNode *fn = um->fn;
			OpCode *op = &fn->u.f.u.op[um->pc];
			RefNode *klass = Value_vp(op->op[0]);
			RefNode *memb = Hash_get_p(&klass->u.c.h, um->memb);
			if (memb != NULL) {
				if (op->type == OP_LITERAL_P) {
					op->op[1] = vp_Value(memb);
					continue;
				} else if (memb->type == NODE_NEW_N || memb->type == NODE_NEW) {
					op->op[0] = vp_Value(memb);
					continue;
				}
			}

			throw_errorf(fs->mod_lang, "DefineError", "%n has no constructor named %r", klass, um->memb);
			add_stack_trace(fn->defined_module, NULL, um->line);
			return FALSE;
		}
	}

	Mem_close(&fv->cmp_mem);
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * sb != NULL : sbに出力、ファイルに出力しない
 * sb == NULL, fh != -1 : fhに出力
 * sb == NULL, fh == -1 : fs->err_dstに出力
 */
void fox_error_dump(StrBuf *sb, FileHandle fh, const RefCharset *cs, int log_style)
{
	StrBuf buf;
	FileHandle fd = -1;
	int sep = log_style ? '|' : '\n';
	char cbuf[24];

	if (sb == NULL) {
		StrBuf_init(&buf, 0);
		sb = &buf;
	}

	if (log_style) {
		time_t i_tm;
		strftime(cbuf, sizeof(cbuf), "%Y-%m-%d %H:%M:%S | ", localtime(&i_tm));
	} else {
		cbuf[0] = '\0';
	}
	if (cs == fs->cs_utf8 || fs->cs_stdio == NULL) {
		StrBuf_add(sb, cbuf, -1);
		stacktrace_to_str(sb, fg->error, sep);
	} else {
		IconvIO ic;
		if (IconvIO_open(&ic, fs->cs_utf8, fs->cs_stdio, "?")) {
			StrBuf buf2;
			StrBuf_init(&buf2, 0);

			StrBuf_add(&buf2, cbuf, -1);
			stacktrace_to_str(&buf2, fg->error, sep);

			IconvIO_conv(&ic, sb, buf2.p, buf2.size, TRUE, FALSE);
			StrBuf_close(&buf2);
			IconvIO_close(&ic);
		} else {
			stacktrace_to_str(sb, fg->error, sep);
		}
	}
	if (log_style) {
		StrBuf_add_c(sb, '\n');
	}
	if (sb == &buf) {
		if (fh == -1) {
			// 絶対パスに直す
			const char *err_path;
			if (is_absolute_path(Str_new(fv->err_dst, -1))) {
				err_path = fv->err_dst;
			} else {
				char *path = Mem_get(&fg->st_mem, fs->cur_dir.size + strlen(fv->err_dst) + 2);
				memcpy(path, fs->cur_dir.p, fs->cur_dir.size);
				strcpy(path + fs->cur_dir.size, fv->err_dst);
				err_path = path;
			}
			fd = open_fox(err_path, O_CREAT|O_WRONLY|O_APPEND, DEFAULT_PERMISSION);
		} else {
			fd = fh;
		}
		write_fox(fd, sb->p, sb->size);
	}

	if (fh != -1) {
		close_fox(fd);
	}
	if (sb == &buf) {
		StrBuf_close(&buf);
	}
}
void fox_close()
{
	if (fs->cgi_mode) {
		send_headers();
	}
	stream_flush_sub(fg->v_cio);
	Mem_close(&fv->cmp_mem);
}

