#include "fox_vm.h"
#include <stdio.h>
#include <string.h>


enum {
	ACCEPT_LANG_MAX = 32,
	LOCALE_LEN = 64,
};

////////////////////////////////////////////////////////////////////////////////

static void make_locale_name(char *dst, int size, Str name)
{
	int i;
	int len = name.size;

	if (len == 1 && name.p[0] == '!') {
		strcpy(dst, "!");
		return;
	}
	if (len > size - 1) {
		len = size - 1;
	}

	for (i = 0; i < len; i++) {
		char c = tolower(name.p[i]);
		if (c == '-' || c == '_') {
			dst[i] = '-';
		} else if (isalnum(c)) {
			dst[i] = c;
		} else {
			break;
		}
	}
	dst[len] = '\0';
}
static RefStr *locale_alias(Str name)
{
	static Hash locales;

	if (locales.entry == NULL) {
		Hash_init(&locales, &fg->st_mem, 64);
		load_aliases_file(&locales, "data" SEP_S "locale.txt");
	}
	return Hash_get(&locales, name.p, name.size);
}


static RefStr **get_best_locale_cgi(const char *p)
{
	int size = 0;
	RefStr **list = Mem_get(&fg->st_mem, ACCEPT_LANG_MAX * sizeof(RefStr*));
	double *qa = malloc(ACCEPT_LANG_MAX * sizeof(double));
	char cbuf[LOCALE_LEN];

	while (*p != '\0') {
		const char *top;
		Str s1;
		double qv = 1.0;

		// ja ; q=1.0,
		while (*p != '\0' && isspace(*p)) {
			p++;
		}
		top = p;
		while (*p != '\0' && *p != ',' && *p != ';') {
			p++;
		}
		s1 = Str_new(top, p - top);

		// タグ名以降を切り出す
		if (*p == ';') {
			p++;
			while (*p != '\0' && isspace(*p)) {
				p++;
			}
			if (p[0] == 'q' && p[1] == '=') {
				p += 2;
				qv = strtod(p, NULL);
			}
			while (*p != '\0' && *p != ',') {
				p++;
			}
		}
		if (*p != '\0') {
			p++;
		}

		make_locale_name(cbuf, LOCALE_LEN, s1);
		// localeのエイリアスの解決
		{
			RefStr *alias = locale_alias(Str_new(cbuf, -1));
			if (alias != NULL) {
				memcpy(cbuf, alias->c, alias->size);
				cbuf[alias->size] = '\0';
			}
		}
		list[size] = intern(cbuf, -1);
		qa[size] = qv;
		size++;
		if (size >= ACCEPT_LANG_MAX - 1) {
			break;
		}
	}

	// 降順ソート
	{
		int i, j;
		for (i = 0; i < size - 1; i++) {
			for (j = i + 1; j < size; j++) {
				if (qa[i] < qa[j]) {
					RefStr *tmp = list[i];
					double dtmp = qa[i];
					list[i] = list[j];
					list[j] = tmp;
					qa[i] = qa[j];
					qa[j] = dtmp;
				}
			}
		}
	}
	list[size] = NULL;
	free(qa);

	return list;
}

/**
 * 優先度の高いlocaleから順番に入っているので
 * 呼び出し元で利用可能なリソースと一致するか調べる
 */
RefStr **get_best_locale_list()
{
	static RefStr **list;

	if (list != NULL) {
		return list;
	}

	if (fs->cgi_mode) {
		const char *lang = Hash_get(&fs->envs, "HTTP_ACCEPT_LANGUAGE", -1);
		if (lang != NULL) {
			return get_best_locale_cgi(lang);
		} else {
			list = Mem_get(&fg->st_mem, sizeof(RefStr*) * 2);
			list[0] = fs->str_0;
			list[1] = NULL;
		}
	} else {
		const char *lang = Hash_get(&fs->envs, "FOX_LANG", -1);
		if (lang == NULL) {
			lang = get_default_locale();
		}
		if (lang != NULL) {
			int i;
			char cbuf[LOCALE_LEN];
			Str s = Str_new(lang, -1);

			// .以降を除去
			for (i = 0; i < s.size; i++) {
				if (s.p[i] == '.') {
					break;
				}
			}
			s.size = i;
			make_locale_name(cbuf, LOCALE_LEN, s);
			// localeのエイリアスの解決
			{
				RefStr *alias = locale_alias(Str_new(cbuf, -1));
				if (alias != NULL) {
					memcpy(cbuf, alias->c, alias->size);
					cbuf[alias->size] = '\0';
				}
			}

			list = Mem_get(&fg->st_mem, sizeof(RefStr*) * 3);
			list[0] = intern(cbuf, -1);
			list[1] = NULL;
		} else {
			// ニュートラル言語
			list = Mem_get(&fg->st_mem, sizeof(RefStr*) * 2);
			list[0] = fs->str_0;
			list[1] = NULL;
		}
	}
	return list;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int load_locale_file(IniTok *tk, Str name)
{
	char *path = str_printf("%S" SEP_S "locale" SEP_S "%S.txt", fs->fox_home, name);
	int ret = IniTok_load(tk, path);
	free(path);
	return ret;
}

static RefLocale *load_locale_sub(RefLocale *loc_src, Str name)
{
	static Hash locales;

	RefLocale *loc;
	IniTok tk;

	if (locales.entry == NULL) {
		Hash_init(&locales, &fg->st_mem, 32);
	}

	loc = Hash_get(&locales, name.p, name.size);
	if (loc != NULL) {
		return loc;
	}

	if (!load_locale_file(&tk, name)) {
		return NULL;
	}

	if (loc_src != NULL) {
		loc = Mem_get(&fg->st_mem, sizeof(RefLocale));
		memcpy(loc, loc_src, sizeof(RefLocale));
	} else {
		loc = Mem_get(&fg->st_mem, sizeof(RefLocale));
		memset(loc, 0, sizeof(*loc));
		loc->rh.type = fs->cls_locale;
		loc->group_n = 3;
	}

	while (IniTok_next(&tk)) {
		if (tk.type == INITYPE_STRING) {
			Str key = tk.key;
			Str val = tk.val;

			switch (key.p[0]) {
			case 'a':
				if (Str_eq_p(key, "am_pm")) {
					Str_split(val, loc->am_pm, lengthof(loc->am_pm), '\t');
				}
				break;
			case 'b':
				if (Str_eq_p(key, "bidi")) {
					if (Str_eq_p(val, "rtl")) {
						loc->rtl = TRUE;
					}
				}
				break;
			case 'd':
				if (Str_eq_p(key, "date")) {
					Str_split(val, loc->date, lengthof(loc->date), '\t');
				} else if (Str_eq_p(key, "decimal")) {
					loc->decimal = str_dup_p(val.p, val.size, &fg->st_mem);
				} else if (Str_eq_p(key, "days")) {
					Str_split(val, loc->week, lengthof(loc->week), '\t');
				} else if (Str_eq_p(key, "days_w")) {
					Str_split(val, loc->week_w, lengthof(loc->week_w), '\t');
				}
				break;
			case 'g':
				if (Str_eq_p(key, "group")) {
					loc->group = str_dup_p(val.p, val.size, &fg->st_mem);
				} else if (Str_eq_p(key, "group_n")) {
					loc->group_n = parse_int(val, 256);
				}
				break;
			case 'l':
				if (Str_eq_p(key, "langtag")) {
					loc->tag = intern(val.p, val.size);
				} else if (Str_eq_p(key, "language")) {
					loc->param[LOCALE_LANGUAGE] = intern(val.p, val.size);
				}
				break;
			case 'm':
				if (Str_eq_p(key, "month")) {
					Str_split(val, loc->month, lengthof(loc->month), '\t');
				} else if (Str_eq_p(key, "month_w")) {
					Str_split(val, loc->month_w, lengthof(loc->month_w), '\t');
				}
				break;
			case 's':
				if (Str_eq_p(key, "script")) {
					loc->param[LOCALE_SCRIPT] = intern(val.p, val.size);
				}
				break;
			case 't':
				if (Str_eq_p(key, "time")) {
					Str_split(val, loc->time, lengthof(loc->time), '\t');
				} else if (Str_eq_p(key, "territory")) {
					loc->param[LOCALE_TERRITORY] = intern(val.p, val.size);
				}
				break;
			case 'v':
				if (Str_eq_p(key, "valiant")) {
					loc->param[LOCALE_VALIANT] = intern(val.p, val.size);
				}
				break;
			}
		}
	}
	IniTok_close(&tk);
	{
		RefStr *p_name = intern(name.p, name.size);
		loc->name = p_name;
		Hash_add_p(&locales, &fg->st_mem, p_name, loc);
	}

	return loc;
}

/**
 * エイリアス解決済みのlocaleを返す
 * 見つからなければデフォルトロケール "!" を返す
 */
static RefLocale *load_locale(Str name)
{
	char buf[LOCALE_LEN];
	RefLocale *loc = NULL;
	int i;

	make_locale_name(buf, LOCALE_LEN, name);

	// デフォルトロケールからコピー
	loc = load_locale_sub(NULL, Str_new("!", 1));
	if (loc == NULL) {
		loc = Mem_get(&fg->st_mem, sizeof(*loc));
		memset(loc, 0, sizeof(*loc));
		loc->name = intern("!", 1);
		loc->rh.type = fs->cls_locale;
		loc->group_n = 3;
	}
	// 長さ0の文字列の場合はデフォルトロケールを返す
	if (buf[0] == '\0') {
		return loc;
	}

	// localeのエイリアスの解決
	if (name.size > 0) {
		RefStr *alias = locale_alias(Str_new(buf, -1));
		if (alias != NULL) {
			memcpy(buf, alias->c, alias->size);
			buf[alias->size] = '\0';
		}
	}
	// -で分割して順番に探す
	name = Str_new(buf, -1);

	// x-hoge などに対応するため、最初の1文字の区間は分割しない
	for (i = 2; i < name.size; i++) {
		if (buf[i] == '-') {
			buf[i] = '\0';
		}
	}

	for (;;) {
		Str name2 = Str_new(buf, -1);
		int found = FALSE;
		RefLocale *loc2 = load_locale_sub(loc, name2);

		if (loc2 == NULL) {
			break;
		}
		loc = loc2;

		for (i = 0; i < name.size; i++) {
			if (buf[i] == '\0') {
				buf[i] = '-';
				found = TRUE;
				break;
			}
		}
		if (!found) {
			break;
		}
	}

	return loc;
}

//////////////////////////////////////////////////////////////////////////////////

static int locale_new(Value *vret, Value *v, RefNode *node)
{
	Str name;
	RefLocale *loc;

	if (fg->stk_top > v + 1) {
		name = Value_str(v[1]);
		if (Str_eq_p(name, "best")) {
			RefStr* const *list = get_best_locale_list();
			while (*list != NULL) {
				loc = load_locale(Str_new((*list)->c, (*list)->size));
				if (loc != fs->loc_neutral) {
					*vret = ref_cp_Value(&loc->rh);
					return TRUE;
				}
				list++;
			}
		}
	} else {
		name = fs->Str_EMPTY;
	}
	loc = load_locale(name);
	*vret = ref_cp_Value(&loc->rh);

	return TRUE;
}
static int locale_marshal_read(Value *vret, Value *v, RefNode *node)
{
	Value r = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
	uint32_t size;
	int rd_size;
	RefLocale *loc;
	char cbuf[64];

	if (!read_int32(&size, r)) {
		return FALSE;
	}
	if (size > 63) {
		throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
		return FALSE;
	}
	rd_size = size;
	if (!stream_read_data(r, NULL, cbuf, &rd_size, FALSE, TRUE)) {
		return FALSE;
	}
	cbuf[rd_size] = '\0';

	loc = load_locale(Str_new(cbuf, -1));
	*vret = ref_cp_Value(&loc->rh);

	return TRUE;
}
static int locale_marshal_write(Value *vret, Value *v, RefNode *node)
{
	RefLocale *loc = Value_vp(*v);
	RefStr *name = loc->tag;
	Value w = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);

	if (!write_int32(name->size, w)) {
		return FALSE;
	}
	if (!stream_write_data(w, name->c, name->size)) {
		return FALSE;
	}

	return TRUE;
}

static void locale_str_word_sub(StrBuf *buf, RefLocale *loc, RefStr *key)
{
	if (loc->locale_name != VALUE_NULL) {
		RefResource *res = Value_vp(loc->locale_name);
		ResEntry *re = Hash_get_p(&res->h, key);
		if (re != NULL && re->type == INITYPE_STRING) {
			StrBuf_add(buf, re->val.p, re->val.size);
		} else {
			StrBuf_add_r(buf, key);
		}
	} else {
		StrBuf_add_r(buf, key);
	}
}
static int locale_str_sub(StrBuf *buf, RefLocale *loc, RefLocale *dsp_loc, int option)
{
	RefStr *language = loc->param[LOCALE_LANGUAGE];
	RefStr *script = loc->param[LOCALE_SCRIPT];
	RefStr *territory = loc->param[LOCALE_TERRITORY];
	RefStr *valiant = loc->param[LOCALE_VALIANT];

	// Resourceが読み込まれていない場合は読み込む
	if (dsp_loc->locale_name == VALUE_NULL) {
		int found = FALSE;
		char *path_p = str_printf("%S" SEP_S "resource" SEP_S "locale", fs->fox_home);
		RefResource *res = new_buf(fs->cls_resource, sizeof(RefResource));
		RefStr *loc_name = dsp_loc->name;
		loc->locale_name = vp_Value(res);
		load_resource(res, &found, Str_new(path_p, -1), Str_new(loc_name->c, loc_name->size), FALSE);
		if (!found) {
			Value_dec(loc->locale_name);
			loc->locale_name = VALUE_NULL;
		}
		free(path_p);
	}

	// language (territory), script, valiant
	if (language != NULL) {
		locale_str_word_sub(buf, dsp_loc, language);
	}
	if (territory != NULL) {
		StrBuf_add(buf, " (", 2);
		locale_str_word_sub(buf, dsp_loc, territory);
		StrBuf_add_c(buf, ')');
	}
	if (option) {
		if (script != NULL) {
			StrBuf_add(buf, ", ", 2);
			locale_str_word_sub(buf, dsp_loc, script);
		}
		if (valiant != NULL) {
			StrBuf_add(buf, ", ", 2);
			locale_str_word_sub(buf, dsp_loc, valiant);
		}
	}
	return TRUE;
}
static int locale_tostr(Value *vret, Value *v, RefNode *node)
{
	RefLocale *loc = Value_vp(*v);
	Str fmt = fs->Str_EMPTY;

	if (fg->stk_top > v + 1) {
		fmt = Value_str(v[1]);
	}
	if (fmt.size == 0) {
		*vret = printf_Value("Locale(%r)", loc->tag);
	} else if (fmt.p[0] == 't') {
		*vret = ref_cp_Value(&loc->tag->rh);
	} else if (fmt.p[0] == 'f') {
		*vret = ref_cp_Value(&loc->name->rh);
	} else if (fmt.p[0] == 'N' || fmt.p[0] == 'n') {
		StrBuf buf;
		RefLocale *dsp_loc = fs->loc_neutral;
		if (fg->stk_top > v + 2) {
			dsp_loc = Value_vp(v[2]);
		}
		StrBuf_init(&buf, 0);
		locale_str_sub(&buf, loc, dsp_loc, fmt.p[0] == 'N');
		*vret = cstr_Value(fs->cls_str, buf.p, buf.size);
		StrBuf_close(&buf);
	} else {
		throw_errorf(fs->mod_lang, "FormatError", "Unknown format string %Q", fmt);
		return FALSE;
	}

	return TRUE;
}
static int locale_tag(Value *vret, Value *v, RefNode *node)
{
	RefLocale *loc = Value_vp(*v);
	*vret = ref_cp_Value(&loc->tag->rh);
	return TRUE;
}
static int locale_filename(Value *vret, Value *v, RefNode *node)
{
	RefLocale *loc = Value_vp(*v);
	*vret = ref_cp_Value(&loc->name->rh);
	return TRUE;
}
static int locale_get_param(Value *vret, Value *v, RefNode *node)
{
	RefLocale *loc = Value_vp(*v);
	int idx = FUNC_INT(node);
	RefStr *rs = loc->param[idx];
	if (rs != NULL) {
		*vret = cstr_Value(fs->cls_str, rs->c, rs->size);
	}
	return TRUE;
}

static int locale_bidi(Value *vret, Value *v, RefNode *node)
{
	RefLocale *loc = Value_vp(*v);
	*vret = cstr_Value(fs->cls_str, (loc->rtl ? "rtl" : "ltr"), 3);
	return TRUE;
}
static void locale_put_array(StrBuf *buf, const char **arr, int size)
{
	enum {
		SEP_SIZE = 3,
	};
	const char *sep = "\",\"";
	int i;

	for (i = 0; i < size; i++) {
		if (i > 0) {
			StrBuf_add(buf, sep, SEP_SIZE);
		}
		add_backslashes_sub(buf, arr[i], -1, ADD_BACKSLASH_U_UCS2);
	}
}
/**
 * localeの情報をjson文字列で返す
 */
static int locale_get_json(Value *vret, Value *v, RefNode *node)
{
	RefLocale *loc = Value_vp(*v);
	StrBuf buf;

	StrBuf_init_refstr(&buf, 0);
	StrBuf_add(&buf, "{\"tag\":\"", -1);
	StrBuf_add_r(&buf, loc->tag);

	StrBuf_add(&buf, "\",\"month\":[\"", -1);
	locale_put_array(&buf, loc->month, lengthof(loc->month));

	StrBuf_add(&buf, "\"],\"month_w\":[\"", -1);
	locale_put_array(&buf, loc->month_w, lengthof(loc->month_w));

	StrBuf_add(&buf, "\"],\"days\":[\"", -1);
	locale_put_array(&buf, loc->week, lengthof(loc->week));

	StrBuf_add(&buf, "\"],\"days_w\":[\"", -1);
	locale_put_array(&buf, loc->week_w, lengthof(loc->week_w));

	StrBuf_add(&buf, "\"],\"date\":[\"", -1);
	locale_put_array(&buf, loc->date, lengthof(loc->date));

	StrBuf_add(&buf, "\"],\"time\":[\"", -1);
	locale_put_array(&buf, loc->time, lengthof(loc->time));

	StrBuf_add(&buf, "\"],\"am\":\"", -1);
	add_backslashes_sub(&buf, loc->am_pm[0], -1, ADD_BACKSLASH_U_UCS2);
	StrBuf_add(&buf, "\",\"pm\":\"", -1);
	add_backslashes_sub(&buf, loc->am_pm[1], -1, ADD_BACKSLASH_U_UCS2);

	StrBuf_add(&buf, "\",\"bidi\":\"", -1);
	StrBuf_add(&buf, (loc->rtl ? "rtl\"}" : "ltr\"}"), 5);

	*vret = StrBuf_str_Value(&buf, fs->cls_str);
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////

/**
 * ResourceLoadErrorは戻り値FALSE
 * ファイルが見つかったかどうかは引数で返す
 */
static int load_resource_sub(RefResource *res, int *found, Mem *mem, Str path, const char *path_p, const char *file_p)
{
	IniTok tk;

	if (!IniTok_load(&tk, path_p)) {
		*found = FALSE;
		return TRUE;
	}

	while (IniTok_next(&tk)) {
		switch (tk.type) {
		case INITYPE_STRING:
		case INITYPE_FILE: {
			Str key = tk.key;

			if (Str_eq_p(key, "locale")) {
				if (res->locale == NULL) {
					res->locale = load_locale(tk.val);
				}
			} else if (Hash_get(&res->h, key.p, key.size) == NULL) {
				if (invalid_utf8_pos(tk.val.p, tk.val.size) >= 0) {
					throw_error_select(THROW_INVALID_UTF8);
					IniTok_close(&tk);
					return FALSE;
				} else {
					ResEntry *re = Mem_get(&res->mem, sizeof(*re));
					re->type = tk.type;

					if (tk.type == INITYPE_STRING) {
						re->val.p = str_dup_p(tk.val.p, tk.val.size, mem);
						re->val.size = tk.val.size;
					} else {
						// ファイルパス
						char *p1 = str_printf("%S" SEP_S "%S" SEP_S "%s", path, key, file_p);
						char *p2 = str_dup_p(p1, -1, mem);
						free(p1);
						re->val = Str_new(p2, -1);
					}

					Hash_add_p(&res->h, &res->mem, intern(key.p, key.size), re);
				}
			}
			break;
		}
		case INITYPE_ERROR:
			throw_errorf(fs->mod_locale, "ResourceError", "Illigal Resource format in %q", path_p);
			IniTok_close(&tk);
			return FALSE;
		}
	}
	if (res->locale == NULL) {
		res->locale = fs->loc_neutral;
	}
	IniTok_close(&tk);
	*found = TRUE;

	return TRUE;
}

/**
 * エラー:return FALSE
 * ファイルが見つからない:pres == NULL
 * res->memは内部で確保した領域がセットされる
 * e == NULLの場合はエラーを返さない
 */
int load_resource(RefResource *res, int *pfound, Str path, Str locale, int no_default)
{
	Mem mem;
	char *path_p;
	char *file_p;
	int found = FALSE;

	*pfound = FALSE;

	Mem_init(&mem, 8 * 1024);
	Hash_init(&res->h, &mem, 16);
	path_p = malloc(path.size + 72);

	memcpy(path_p, path.p, path.size);
	path_p[path.size] = SEP_C;
	file_p = path_p + path.size + 1;

	make_locale_name(file_p, 64, locale);
	{
		RefStr *alias = locale_alias(Str_new(file_p, -1));
		if (alias != NULL) {
			memcpy(file_p, alias->c, alias->size);
			file_p[alias->size] = '\0';
		}
	}
	strcat(file_p, ".txt");

	for (;;) {
		char *p;

		if (!load_resource_sub(res, &found, &mem, path, path_p, file_p)) {
			goto ERROR_END;
		}
		if (found) {
			*pfound = TRUE;
		}

		// ja-jp.txt -> ja.txt
		p = file_p + strlen(file_p) - 1;
		for (; *p != '-'; p--) {
			if (*p == SEP_C) {
				// おわり
				if (!no_default || *pfound) {
					strcpy(file_p, "!.txt");
					if (!load_resource_sub(res, &found, &mem, path, path_p, file_p)) {
						goto ERROR_END;
					}
					if (found) {
						*pfound = TRUE;
					}
				}
				free(path_p);
				if (*pfound) {
					res->mem = mem;
				} else {
					Mem_close(&mem);
				}
				return TRUE;
			}
		}
		strcpy(p, ".txt");
	}

ERROR_END:
	Mem_close(&mem);
	return FALSE;
}
/**
 * エラー:return FALSE
 * ファイルが見つからない:pres == NULL
 */
static int load_best_resource(RefResource *res, int *found, Str path)
{
	RefStr* const *list = get_best_locale_list();
	RefStr* const *pp = list;
	RefStr *rs;

	while (*pp != NULL) {
		rs = *pp;
		if (!load_resource(res, found, path, Str_new(rs->c, rs->size), TRUE)) {
			return FALSE;
		}
		if (*found) {
			return TRUE;
		}
		pp++;
	}

	rs = *list;
	return load_resource(res, found, path, Str_new(rs->c, rs->size), FALSE);
}

int Resource_get(Str *ret, RefResource *res, Str name)
{
	ResEntry *re = Hash_get(&res->h, name.p, name.size);

	if (re != NULL) {
		if (re->type == INITYPE_FILE) {
			// ファイルから読み取り
			int fd = open_fox(re->val.p, O_RDONLY, DEFAULT_PERMISSION);
			int64_t size = get_file_size(fd);
			if (size < 0) {
				throw_error_select(THROW_CANNOT_OPEN_FILE__STR, re->val);
				return FALSE;
			} else if (size == 0) {
				*ret = fs->Str_EMPTY;
			} else {
				char *buf = Mem_get(&res->mem, size);
				int read_size = read_fox(fd, buf, size);
				re->val = Str_new(buf, read_size);
				re->type = INITYPE_STRING;
			}
			if (fd != -1) {
				close_fox(fd);
			}
		}
		*ret = re->val;
		return TRUE;
	} else {
		throw_errorf(fs->mod_lang, "IndexError", "Resource key %Q not found", name);
		return FALSE;
	}
}
void Resource_close(RefResource *res)
{
	Mem_close(&res->mem);
	free(res);
}

static int resource_new(Value *vret, Value *v, RefNode *node)
{
	Str name_s = Value_str(v[1]);
	Value v2 = v[2];
	const RefNode *v2_type = Value_type(v2);
	Str loc;
	char *path;

	// 型チェックを先に行う
	if (v2_type == fs->cls_locale) {
		RefLocale *lc = Value_vp(v2);
		loc = Str_new(lc->name->c, lc->name->size);
	} else if (v2_type == fs->cls_str) {
		loc = Value_str(v2);
	} else {
		throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_locale, fs->cls_str, v2_type, 2);
		return FALSE;
	}

	path = resource_to_path(name_s, SEP_S "!.txt");
	if (path == NULL) {
		return FALSE;
	} else {
		int found;
		Str path_s = Str_new(path, -1);
		RefResource *res = new_buf(fs->cls_resource, sizeof(RefResource));
		*vret = vp_Value(res);

		// pathから"/!.txt"を削除
		path_s.size -= 6;

		if (Str_eq_p(loc, "best")) {
			if (!load_best_resource(res, &found, path_s)) {
				free(path);
				return FALSE;
			}
		} else {
			if (!load_resource(res, &found, path_s, loc, FALSE)) {
				free(path);
				return FALSE;
			}
		}
		if (!found) {
			throw_errorf(fs->mod_io, "FileOpenError", "Cannot find file for %S", name_s);
			free(path);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * 文字列をそのまま返す
 */
static int resource_getstr(Value *vret, Value *v, RefNode *node)
{
	RefResource *res = Value_vp(*v);
	Str key = Value_str(v[1]);
	Str ret_s;

	if (!Resource_get(&ret_s, res, key)) {
		return FALSE;
	}
	*vret = cstr_Value(fs->cls_str, ret_s.p, ret_s.size);

	return TRUE;
}
static int resource_locale(Value *vret, Value *v, RefNode *node)
{
	RefResource *res = Value_vp(*v);
	RefLocale *loc = res->locale;

	if (loc != NULL) {
		*vret = ref_cp_Value(&loc->rh);
	}
	return TRUE;
}
static int resource_add(Value *vret, Value *v, RefNode *node)
{
	RefResource *res = Value_vp(*v);
	RefResource *res2 = Value_vp(v[1]);
	int i;

	for (i = 0; i < res2->h.entry_num; i++) {
		HashEntry *he = res2->h.entry[i];
		while (he != NULL) {
			HashEntry *h2 = Hash_get_add_entry(&res->h, &res->mem, he->key);
			ResEntry *re = he->p;
			ResEntry *re2 = h2->p;
			if (re2 == NULL) {
				char *cbuf = Mem_get(&res->mem, re->val.size + 1);
				memcpy(cbuf, re->val.p, re->val.size);
				cbuf[re->val.size] = '\0';  // ファイルパスの場合NUL終端でなければならない

				re2 = Mem_get(&res->mem, sizeof(*re2));
				re2->type = re->type;
				re2->val = Str_new(cbuf, re->val.size);
				h2->p = re2;
			}
			he = he->next;
		}
	}
	return TRUE;
}
static int resource_dispose(Value *vret, Value *v, RefNode *node)
{
	RefResource *res = Value_vp(*v);
	Mem_close(&res->mem);
	return TRUE;
}

static int resource_file(Value *vret, Value *v, RefNode *node)
{
	char *path;
	Str name_s = Value_str(v[1]);
	Str ext_s = Value_str(v[2]);
	char *ext = malloc(ext_s.size + 4);

	ext[0] = '.';
	strcpy(ext + 1, ext_s.p);
	path = resource_to_path(name_s, ext);
	free(ext);

	if (path == NULL) {
		return FALSE;
	}
	*vret = cstr_Value(fs->cls_file, path, -1);
	free(path);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////

static void define_locale_func(RefNode *m)
{
	RefNode *n;

	n = define_identifier(m, m, "resource_file", NODE_FUNC_N, 0);
	define_native_func_a(n, resource_file, 2, 2, NULL, fs->cls_str, fs->cls_str);
}
static void define_locale_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

	fs->loc_neutral = load_locale(fs->Str_EMPTY);

	// Locale
	redefine_identifier(m, m, "Locale", NODE_CLASS, 0, fs->cls_locale);
	cls = fs->cls_locale;
	n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	define_native_func_a(n, locale_new, 0, 1, NULL, fs->cls_str);
	n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
	define_native_func_a(n, locale_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

	n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	define_native_func_a(n, locale_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
	define_native_func_a(n, locale_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

	n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	define_native_func_a(n, object_eq, 1, 1, NULL, fs->cls_locale);
	n = define_identifier(m, cls, "tag", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_tag, 0, 0, NULL);
	n = define_identifier(m, cls, "filename", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_filename, 0, 0, NULL);
	n = define_identifier(m, cls, "language", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_get_param, 0, 0, (void*)LOCALE_LANGUAGE);
	n = define_identifier(m, cls, "script", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_get_param, 0, 0, (void*)LOCALE_SCRIPT);
	n = define_identifier(m, cls, "territory", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_get_param, 0, 0, (void*)LOCALE_TERRITORY);
	n = define_identifier(m, cls, "valiant", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_get_param, 0, 0, (void*)LOCALE_VALIANT);

	n = define_identifier(m, cls, "bidi", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_bidi, 0, 0, NULL);
	n = define_identifier(m, cls, "json", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, locale_get_json, 0, 0, NULL);

	n = define_identifier(m, cls, "NEUTRAL", NODE_CONST, 0);
	n->u.k.val = ref_cp_Value(&fs->loc_neutral->rh);

	cls->u.c.n_memb = 2;
	extends_method(cls, fs->cls_obj);


	// Resource
	cls = define_identifier(m, m, "Resource", NODE_CLASS, 0);
	fs->cls_resource = cls;
	n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	define_native_func_a(n, resource_new, 2, 2, NULL, fs->cls_str, NULL);
	n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
	define_native_func_a(n, resource_dispose, 0, 0, NULL);

	n = define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
	define_native_func_a(n, resource_getstr, 1, 1, NULL, fs->cls_str);
	n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	define_native_func_a(n, resource_getstr, 1, 1, NULL, fs->cls_str);
	n = define_identifier(m, cls, "locale", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, resource_locale, 0, 0, NULL);
	n = define_identifier(m, cls, "add", NODE_FUNC_N, 0);
	define_native_func_a(n, resource_add, 1, 1, NULL, fs->cls_resource);

	extends_method(cls, fs->cls_obj);


	cls = define_identifier(m, m, "ResourceError", NODE_CLASS, 0);
	define_error_class(cls, fs->cls_error, m);
}

void init_locale_module_1()
{
	RefNode *m = new_sys_Module("locale");

	define_locale_class(m);
	define_locale_func(m);
	m->u.m.loaded = TRUE;
	fs->mod_locale = m;
}
