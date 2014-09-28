#define DEFINE_GLOBALS
#include "fox_vm.h"
#include "datetime.h"

#ifndef WIN32
#include <dlfcn.h>
#endif


static const char *tbegin = "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\">\n<tbody>\n<tr><th>";
static const char *tlend = "</td></tr>\n<tr><th>";
static const char *tend = "</td></tr>\n</tbody>\n</table>\n";


static void write_html_encode(const char *p, int size)
{
	StrBuf sb;

	StrBuf_init(&sb, 0);
	convert_html_entity(&sb, p, size, TRUE);
	stream_write_data(fg->v_cio, sb.p, sb.size);
	StrBuf_close(&sb);
}

static void show_version(void)
{
	char cbuf[32];

	stream_write_data(fg->v_cio, "<h1>Programming language fox</h1>\n", -1);
	stream_write_data(fg->v_cio, tbegin, -1);
	stream_write_data(fg->v_cio, "Version</th><td>", -1);

	sprintf(cbuf, "%d.%d.%d", FOX_VERSION_MAJOR, FOX_VERSION_MINOR, FOX_VERSION_REVISION);
	stream_write_data(fg->v_cio, cbuf, -1);
#ifndef NO_DEBUG
	stream_write_data(fg->v_cio, " (debug-build)", -1);
#endif

	stream_write_data(fg->v_cio, tlend, -1);
	stream_write_data(fg->v_cio, "Architecture</th><td>", -1);
	sprintf(cbuf, "%dbit", (int)sizeof(void*) * 8);
	stream_write_data(fg->v_cio, cbuf, -1);

	stream_write_data(fg->v_cio, tlend, -1);
	stream_write_data(fg->v_cio, "Build at</th><td>", -1);
	stream_write_data(fg->v_cio, __DATE__, -1);

	stream_write_data(fg->v_cio, tlend, -1);
	stream_write_data(fg->v_cio, "PCRE</th><td>", -1);
	stream_write_data(fg->v_cio, pcre_version(), -1);

	stream_write_data(fg->v_cio, tlend, -1);
	stream_write_data(fg->v_cio, "fox home</th><td>", -1);
	write_html_encode(fs->fox_home.p, fs->fox_home.size);

	stream_write_data(fg->v_cio, tend, -1);
}
/**
 * ライブラリのバージョン
 */
static int string_sort(const void *p1, const void *p2)
{
	const char *s1 = *(const char **)p1;
	const char *s2 = *(const char **)p2;

	for (;;) {
		int c1 = *s1;
		int c2 = *s2;
		if (c1 == '.') {
			c1 = '\0';
		}
		if (c2 == '.') {
			c2 = '\0';
		}
		if (c1 != c2) {
			return c1 - c2;
		}
		if (c1 == '\0') {
			return *s2 - *s1;
		}
		s1++;
		s2++;
	}
}
static int str_eq_end(const char *s, const char *p)
{
	int slen = strlen(s);
	int plen = strlen(p);

	if (slen >= plen) {
		return strcmp(s + slen - plen, p) == 0;
	}
	return FALSE;
}
/**
 * 英数字のみで構成されている
 */
static int str_is_subdir(const char *s)
{
	while (*s != '\0') {
		int c = *s++;
		if (!isdigit_fox(c) && !islower_fox(c)) {
			return FALSE;
		}
	}
	return TRUE;
}

static void show_lib_version_sub(Mem *mem, StrBuf *path, StrBuf *package, const char *title)
{
	struct dirent *dh;
	DIR *d;
	StrBuf files;   // 文字列のポインタの配列として使う
	const char **files_p;
	int files_size;
	int path_size = path->size;
	int package_size = package->size;
	int i;

	StrBuf_init(&files, 0);

	// ライブラリのリストを取得
	path->p[path->size - 1] = '\0';
	d = opendir_fox(path->p);
	if (d == NULL) {
		return;
	}
	path->p[path->size - 1] = SEP_C;

	while ((dh = readdir_fox(d)) != NULL) {
		const char *name = dh->d_name;
		if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
			char *p = str_dup_p(name, -1, mem);
			StrBuf_add(&files, (const char*)&p, sizeof(p));
		}
	}
	closedir_fox(d);

	files_p = (const char**)files.p;
	files_size = files.size / sizeof(const char*);
	// ライブラリのリストをソート
	qsort(files_p, files_size, sizeof(const char*), string_sort);

	for (i = 0; i < files_size; i++) {
		const char *name = files_p[i];

		if (str_eq_end(name, ".so")) {
			FoxModuleVersion module_version;
			void *handle;

			StrBuf_add(path, name, -1);
			StrBuf_add_c(path, '\0');
			StrBuf_add(package, name, -1);
			package->p[package->size - 3] = '\0';

			handle = dlopen_fox(path->p, RTLD_LAZY);

			if (handle == NULL) {
				stream_write_data(fg->v_cio, "<h1>", -1);
				stream_write_data(fg->v_cio, title, -1);
				stream_write_data(fg->v_cio, package->p, -1);
				stream_write_data(fg->v_cio, "</h1>\n", -1);
				stream_write_data(fg->v_cio, "<p>", -1);
				stream_write_data(fg->v_cio, dlerror_fox(), -1);
				stream_write_data(fg->v_cio, "</p>\n", -1);
				path->size = path_size;
				package->size = package_size;
				continue;
			}
			module_version = dlsym_fox(handle, "module_version");

			if (module_version != NULL) {
				const char *libver = module_version(fs);
				const char *p = libver;

				stream_write_data(fg->v_cio, "<h1>", -1);
				stream_write_data(fg->v_cio, title, -1);
				stream_write_data(fg->v_cio, package->p, -1);
				stream_write_data(fg->v_cio, "</h1>\n", -1);
				stream_write_data(fg->v_cio, tbegin, -1);
				while (*p != '\0') {
					while (*p != '\0' && *p != '\t') {
						p++;
					}
					write_html_encode(libver, p - libver);
					stream_write_data(fg->v_cio, "</th><td>", -1);
					if (*p != '\0') {
						p++;
					}
					libver = p;
					while (*p != '\0' && *p != '\n') {
						p++;
					}
					write_html_encode(libver, p - libver);
					if (*p != '\0') {
						p++;
					}
					libver = p;

					if (*p != '\0') {
						stream_write_data(fg->v_cio, tlend, -1);
					}
					stream_write_data(fg->v_cio, "\n", 1);
				}
				stream_write_data(fg->v_cio, tend, -1);
			}
		} else if (str_is_subdir(name)) {
			StrBuf_add(path, name, -1);
			StrBuf_add_c(path, SEP_C);
			StrBuf_add(package, name, -1);
			StrBuf_add_c(package, '.');
			show_lib_version_sub(mem, path, package, title);
		}
		path->size = path_size;
		package->size = package_size;
	}
	StrBuf_close(&files);
}
static void show_lib_version(void)
{
	StrBuf path, package;
	Mem mem;

	init_so_func();

	Mem_init(&mem, 8 * 1024);
	StrBuf_init(&path, 0);
	StrBuf_init(&package, 0);

	StrBuf_add(&path, fs->fox_home.p, fs->fox_home.size);
	StrBuf_add(&path, SEP_S "import" SEP_S, -1);
	show_lib_version_sub(&mem, &path, &package, "Native library: ");

	Mem_close(&mem);
}

static void show_configure_env(const char *key, int string)
{
	const char *p;
	int err;
	stream_write_data(fg->v_cio, tlend, -1);

	stream_write_data(fg->v_cio, key, -1);
	stream_write_data(fg->v_cio, "</th><td>", -1);

	p = Hash_get(&fs->envs, key, -1);
	if (p != NULL) {
		if (string) {
			err = !is_string_only_ascii(p, -1, " =;,");
		} else {
			err = FALSE;
		}
		if (err) {
			stream_write_data(fg->v_cio, "<span class=\"err\">", -1);
		}
		write_html_encode(p, -1);
		if (err) {
			stream_write_data(fg->v_cio, "</span>", -1);
		}
	}
}
static void show_configure_path(PtrList *lst, const char *title)
{
	int first = TRUE;

	stream_write_data(fg->v_cio, title, -1);
	for (; lst != NULL; lst = lst->next) {
		int exists = exists_file(lst->u.c) == EXISTS_DIR;
		if (!first) {
			first = FALSE;
			stream_write_data(fg->v_cio, "<br>", -1);
		}
		if (!exists) {
			stream_write_data(fg->v_cio, "<span class=\"err\">", -1);
		}
		write_html_encode(lst->u.c, -1);
		if (!exists) {
			stream_write_data(fg->v_cio, "</span>", -1);
		}
	}
	stream_write_data(fg->v_cio, tlend, -1);
}
static void show_configure(void)
{
	const char *p;
	RefTimeZone *tz = NULL;
	int default_timezone = FALSE;
	int tz_error = FALSE;
	char ctmp[48];
	int defs[ENVSET_NUM];

	memset(defs, 0, sizeof(defs));

	p = Hash_get(&fs->envs, "FOX_TZ", -1);
	if (p != NULL) {
		tz = load_timezone(p, -1);
		if (tz == NULL) {
			tz_error = TRUE;
		}
	}
	if (tz == NULL) {
		tz = get_machine_localtime();
		default_timezone = TRUE;
	}
	load_env_settings(defs);

	stream_write_data(fg->v_cio, "<h1>Configuration</h1>\n", -1);
	stream_write_data(fg->v_cio, tbegin, -1);

	stream_write_data(fg->v_cio, "FOX_ERROR</th><td>", -1);

	p = Hash_get(&fs->envs, "FOX_ERROR", -1);
	if (defs[ENVSET_ERROR]) {
		write_html_encode(fv->err_dst, -1);
	} else {
		stream_write_data(fg->v_cio, "<span class=\"def\">", -1);
		write_html_encode(fv->err_dst, -1);
		stream_write_data(fg->v_cio, "</span>", -1);
	}
	stream_write_data(fg->v_cio, tlend, -1);

	stream_write_data(fg->v_cio, "FOX_LANG</th><td>", -1);
	p = Hash_get(&fs->envs, "FOX_LANG", -1);
	if (p != NULL) {
		write_html_encode(p, -1);
	} else {
		stream_write_data(fg->v_cio, "<span class=\"def\">", -1);
		write_html_encode(get_default_locale(), -1);
		stream_write_data(fg->v_cio, "</span>", -1);
	}
	stream_write_data(fg->v_cio, tlend, -1);

	stream_write_data(fg->v_cio, "FOX_MAX_ALLOC</th><td>", -1);
	if (defs[ENVSET_MAX_ALLOC]) {
		sprintf(ctmp, "%d KB", fs->max_alloc / 1024);
	} else {
		sprintf(ctmp, "<span class=\"def\">%d KB</span>", fs->max_alloc / 1024);
	}
	stream_write_data(fg->v_cio, ctmp, -1);
	stream_write_data(fg->v_cio, tlend, -1);

	stream_write_data(fg->v_cio, "FOX_MAX_STACK</th><td>", -1);
	if (defs[ENVSET_MAX_STACK]) {
		sprintf(ctmp, "%d", fs->max_stack);
	} else {
		sprintf(ctmp, "<span class=\"def\">%d</span>", fs->max_stack);
	}
	stream_write_data(fg->v_cio, ctmp, -1);
	stream_write_data(fg->v_cio, tlend, -1);

	show_configure_path(import_path, "FOX_IMPORT</th><td>");
	show_configure_path(resource_path, "FOX_RESOURCE</th><td>");

	stream_write_data(fg->v_cio, "FOX_STDIO_CHARSET</th><td>", -1);
	if (defs[ENVSET_STDIO_CHARSET]) {
		RefStr *rs = fs->cs_stdio->name;
		stream_write_data(fg->v_cio, rs->c, rs->size);
	} else {
		RefStr *rs = fs->cs_stdio->name;
		stream_write_data(fg->v_cio, "<span class=\"def\">", -1);
		stream_write_data(fg->v_cio, rs->c, rs->size);
		stream_write_data(fg->v_cio, "</span>", -1);
	}
	stream_write_data(fg->v_cio, tlend, -1);

	stream_write_data(fg->v_cio, "FOX_TZ</th><td>", -1);
	if (default_timezone) {
		if (tz != NULL) {
			RefStr *rs = tz->name;
			if (tz_error) {
				stream_write_data(fg->v_cio, "<span class=\"err\">", -1);
			} else {
				stream_write_data(fg->v_cio, "<span class=\"def\">", -1);
			}
			stream_write_data(fg->v_cio, rs->c, rs->size);
			stream_write_data(fg->v_cio, "</span>", -1);
		} else {
			stream_write_data(fg->v_cio, "<span class=\"def\">(Etc/UTC)</span>", -1);
		}
	} else {
		RefStr *rs = tz->name;
		stream_write_data(fg->v_cio, rs->c, rs->size);
	}

	show_configure_env("FOX_COOKIE_DOMAIN", TRUE);
	show_configure_env("FOX_COOKIE_PATH", TRUE);
	show_configure_env("FOX_COOKIE_SECURE", FALSE);
	show_configure_env("FOX_COOKIE_HTTPONLY", FALSE);

	stream_write_data(fg->v_cio, tend, -1);
}

/**
 * 環境変数をソートして表示
 */
static int hashentry_sort(const void *p1, const void *p2)
{
	RefStr *r1 = (*(HashEntry**)p1)->key;
	RefStr *r2 = (*(HashEntry**)p2)->key;
	int len = r1->size < r2->size ? r1->size : r2->size;
	int cmp = memcmp(r1->c, r2->c, len);
	if (cmp == 0) {
		cmp = r1->size - r2->size;
	}
	return cmp;
}
static void show_environment(void)
{
	int i;
	int count = fs->envs.count;
	HashEntry **ph = malloc(sizeof(HashEntry*) * count);
	int n = 0;

	for (i = 0; i < fs->envs.entry_num; i++) {
		HashEntry *he = fs->envs.entry[i];
		for (; he != NULL; he = he->next) {
			ph[n++] = he;
		}
	}
	qsort(ph, count, sizeof(HashEntry*), hashentry_sort);

	stream_write_data(fg->v_cio, "<h1>Environment</h1>\n", -1);
	stream_write_data(fg->v_cio, tbegin, -1);

	for (i = 0; i < count; i++) {
		HashEntry *he = ph[i];
		if (i > 0) {
			stream_write_data(fg->v_cio, tlend, -1);
		}
		write_html_encode(he->key->c, he->key->size);
		stream_write_data(fg->v_cio, "</th><td>", -1);
		write_html_encode(he->p, -1);
	}
	stream_write_data(fg->v_cio, tend, -1);

	free(ph);
}

void print_foxinfo()
{
	const char *header = 
		"Content-Type: text/html; charset=UTF-8\n"
		"\n"
		"<!doctype html>\n"
		"<html>\n<head>\n"
		"<meta charset=\"UTF-8\">\n"
		"<style type=\"text/css\">\n"
		"body{color:black;background:#fcfcfc;}\n"
		"h1,h2{text-align:center;}\n"
		"h1{font-size:120%;}\n"
		"table{font-size:90%;width:600px;margin:auto;}\n"
		"th,td{border:1px solid;border-color:white gray gray white;padding:2px 4px;}\n"
		"th{width:50%;background:#ccf;}\n"
		"td{width:50%;background:#ddd;}\n"
		".err{color:red;}\n"
		".def{color:#666;}\n"
		"</style>\n"
		"<title>foxinfo</title>\n"
		"</head>\n<body>\n";
	const char *footer = "</body>\n</html>\n";

	headers_sent = TRUE;

	stream_write_data(fg->v_cio, header, -1);
	show_version();
	show_lib_version();
	show_configure();
	show_environment();
	stream_write_data(fg->v_cio, footer, -1);
}

static void cin_flush(void)
{
	// 標準入力を全て読む
	const char *method = Hash_get(&fs->envs, "REQUEST_METHOD", -1);
	if (method != NULL && str_eqi(method, -1, "POST", 4)) {
		char buf[BUFFER_SIZE];
		while (read_fox(STDIN_FILENO, buf, BUFFER_SIZE) > 0) {
		}
	}
}

void print_last_error()
{
	if (fg->error != VALUE_NULL && strcmp(fv->err_dst, "null") != 0) {
		if (strcmp(fv->err_dst, "stdout") == 0) {
			StrBuf sb;
			StrBuf sb2;

			StrBuf_init(&sb, 0);
			StrBuf_init(&sb2, 0);

			if (is_content_type_html()) {
				fox_error_dump(&sb, -1, fs->cs_utf8, FALSE);
				convert_html_entity(&sb2, sb.p, sb.size, TRUE);
			} else {
				fox_error_dump(&sb2, -1, fs->cs_utf8, FALSE);
			}

			send_headers();
			if (fs->cs_stdio == fs->cs_utf8) {
				stream_write_data(fg->v_cio, sb2.p, sb2.size);
			} else {
				sb.size = 0;
				convert_str_to_bin_sub(&sb, sb2.p, sb2.size, fs->cs_stdio, "?");
				stream_write_data(fg->v_cio, sb.p, sb.size);
			}

			StrBuf_close(&sb2);
			StrBuf_close(&sb);
		} else if (strcmp(fv->err_dst, "stderr") == 0) {
			fox_error_dump(NULL, STDERR_FILENO, fs->cs_stdio, TRUE);
		} else {
			fox_error_dump(NULL, -1, fs->cs_utf8, TRUE);
		}
	}
}
/**
 * ヘッダも含めて表示
 */
static void print_404()
{
	const char *msg = "Status: 404\n"
			"Content-Type: text/html\n"
			"\n"
			"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
			"<html><head>\n"
			"<title>404 Not Found</title>\n"
			"</head><body>\n"
			"<h1>Not Found</h1>\n"
			"<p>The requested URL was not found on this server.</p>\n"
			"</body></html>\n";
	write_p(STDOUT_FILENO, msg);
}

void put_log(const char *msg, int msg_size);
/**
 * argv[1]にスクリプト名
 */
int main_fox(int argc, const char **argv)
{
	RefNode *mod;
	RefStr *name_startup;
	int ret = FALSE;
	const char *srcfile;

	init_stdio();

	// デフォルト値
	fs->cgi_mode = TRUE;
	fs->max_alloc = 32 * 1024 * 1024; // 32MB
	fs->max_stack = 32768;
	fv->err_dst = "stdout";
	fs->cs_stdio = get_console_charset();

	fox_init_compile(FALSE);

	if (argv[1] != NULL) {
		srcfile = split_script_and_pathinfo(argv[1]);
		fs->cur_dir = base_dir_with_sep(Str_new(srcfile, -1));
		load_htfox();
		mod = get_module_by_file(srcfile);
		if (mod != NULL) {
			mod->u.m.src_path = srcfile;
		}
		fv->argc = argc - 1;
		fv->argv = argv + 1;
	} else {
		print_404();
		return FALSE;
	}

	// ソースを読み込んだ後で実行
	cgi_init_responce();
	init_fox_stack();

	if (mod == NULL) {
		throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(argv[1], -1));
		goto ERROR_END;
	}
	if (fg->error != VALUE_NULL) {
		goto ERROR_END;
	}

	// 参照できない名前で登録
	name_startup = intern("[startup]", 9);
	mod->name = name_startup;
	Hash_add_p(&fg->mod_root, &fg->st_mem, name_startup, mod);

	if (!fox_link()) {
		goto ERROR_END;
	}
	if (!fox_run(mod)) {
		goto ERROR_END;
	}
	ret = TRUE;

ERROR_END:
	print_last_error();
	cin_flush();
	fox_close();
	return ret;
}
