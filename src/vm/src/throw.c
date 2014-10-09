#include "fox_parse.h"

void throw_stopiter()
{
	if (fg->error != VALUE_NULL) {
		Value_dec(fg->error);
	}
	fg->error = vp_Value(ref_new(fs->cls_stopiter));
}

void throw_error_vprintf(RefNode *err_m, const char *err_name, const char *fmt, va_list va)
{
	RefNode *err = Hash_get_p(&err_m->u.m.h, intern(err_name, -1));
	Ref *r;

	if (err == NULL) {
		const RefStr *m = err_m->name;
		fatal_errorf(NULL, "ClassNotFound:%r.%s", m, err_name);
	}

	if (fg->error != VALUE_NULL) {
		Value_dec(fg->error);
	}
	r = ref_new(err);
	fg->error = vp_Value(r);

	if (fmt != NULL) {
		StrBuf msg;
		StrBuf_init(&msg, 0);
		StrBuf_vprintf(&msg, fmt, va);
		r->v[1] = cstr_Value(fs->cls_str, msg.p, msg.size);
		StrBuf_close(&msg);
	}
}
void fatal_errorf(RefNode *fn, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	throw_error_vprintf(fs->mod_lang, "FatalError", fmt, va);
	va_end(va);

	if (fn != NULL) {
		add_stack_trace(NULL, fn, -1);
	}
	print_last_error();
	fox_close();
	exit(0);
}
void throw_errorf(RefNode *err_m, const char *err_name, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	throw_error_vprintf(err_m, err_name, fmt, va);
	va_end(va);
}
void throw_error_select(int err_type, ...)
{
	RefNode *err_m = NULL;
	const char *err_name = NULL;
	const char *fmt = NULL;

	switch (err_type) {
	case THROW_NOT_DEFINED__REFSTR:
		err_m = fs->mod_lang;
		err_name = "DefineError";
		fmt = "%r is not defined";
		break;
	case THROW_MAX_ALLOC_OVER__INT:
		err_m = fs->mod_lang;
		err_name = "MemoryError";
		fmt = "Allocated size is greater than MAX_ALLOC (%d)";
		break;
	case THROW_ARGMENT_TYPE__NODE_NODE_INT:
		err_m = fs->mod_lang;
		err_name = "TypeError";
		fmt = "%n required but %n (argument #%d)";
		break;
	case THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT:
		err_m = fs->mod_lang;
		err_name = "TypeError";
		fmt = "%n or %n required but %n (argument #%d)";
		break;
	case THROW_INVALID_UTF8:
		err_m = fs->mod_lang;
		err_name = "CharsetError";
		fmt = "Invalid UTF-8 sequence";
		break;
	case THROW_LOOP_REFERENCE:
		err_m = fs->mod_lang;
		err_name = "LoopReferenceError";
		fmt = "Loop reference detected";
		break;
	case THROW_CANNOT_OPEN_FILE__STR:
		err_m = fs->mod_file;
		err_name = "FileOpenError";
		fmt = "Cannot open file %Q";
		break;
	case THROW_FLOAT_VALUE_OVERFLOW:
		err_m = fs->mod_lang;
		err_name = "FloatError";
		fmt = "Float value overflow";
		break;
	case THROW_NOT_OPENED_FOR_READ:
		err_m = fs->mod_io;
		err_name = "ReadError";
		fmt = "Not opened for read";
		break;
	case THROW_NOT_OPENED_FOR_WRITE:
		err_m = fs->mod_io;
		err_name = "WriteError";
		fmt = "Not opened for write";
		break;
	case THROW_NO_MEMBER_EXISTS__NODE_REFSTR:
		err_m = fs->mod_lang;
		err_name = "NameError";
		fmt = "%n has no member named %r";
		break;
	case THROW_CANNOT_MODIFY_ON_ITERATION:
		err_m = fs->mod_lang;
		err_name = "IlligalOperationError";
		fmt = "Cannot modify any elements on iteration";
		break;
	case THROW_INVALID_INDEX__VAL_INT:
		err_m = fs->mod_lang;
		err_name = "IndexError";
		fmt = "Invalid index number (%v of %d)";
		break;
	default:
		fatal_errorf(NULL, "Unknown error code %d (throw_error_select)", err_type);
		break;
	}

	va_list va;
	va_start(va, err_type);
	throw_error_vprintf(err_m, err_name, fmt, va);
	va_end(va);
}
