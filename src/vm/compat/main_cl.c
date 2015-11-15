#define DEFINE_GLOBALS
#include "fox_vm.h"
#include "datetime.h"
#include <pcre.h>

#ifndef WIN32
#include <dlfcn.h>
#endif


typedef struct {
#ifdef DEBUGGER
    int syntax_only;
#endif
    const char *srcfile;
    const char *src_raw;
} ArgumentInfo;


/**
 * fox本体と、利用しているライブラリのバージョン
 */
static void show_version(void)
{
    char cbuf[32];

    stream_write_data(fg->v_cio, "[Programming language fox]\nVersion: ", -1);

    sprintf(cbuf, "%d.%d.%d", FOX_VERSION_MAJOR, FOX_VERSION_MINOR, FOX_VERSION_REVISION);
    stream_write_data(fg->v_cio, cbuf, -1);
#ifndef NO_DEBUG
    stream_write_data(fg->v_cio, " (debug-build)", -1);
#endif

    stream_write_data(fg->v_cio, "\nArchitecture: ", -1);
    sprintf(cbuf, "%dbit", (int)sizeof(void*) * 8);
    stream_write_data(fg->v_cio, cbuf, -1);

    stream_write_data(fg->v_cio, "\nBuild at: ", -1);
    stream_write_data(fg->v_cio, __DATE__, -1);

    stream_write_data(fg->v_cio, "\nPCRE: ", -1);
    stream_write_data(fg->v_cio, pcre_version(), -1);

    stream_write_data(fg->v_cio, "\nfox home: ", -1);
    stream_write_data(fg->v_cio, fs->fox_home->c, fs->fox_home->size);

    stream_write_data(fg->v_cio, "\n", 1);
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
                stream_write_data(fg->v_cio, "[", 1);
                stream_write_data(fg->v_cio, title, -1);
                stream_write_data(fg->v_cio, package->p, -1);
                stream_write_data(fg->v_cio, "]\n", 2);
                stream_write_data(fg->v_cio, dlerror_fox(), -1);
                stream_write_data(fg->v_cio, "\n", 1);
                path->size = path_size;
                package->size = package_size;
                continue;
            }
            module_version = dlsym_fox(handle, "module_version");

            if (module_version != NULL) {
                const char *libver = module_version(fs);
                const char *p = libver;

                stream_write_data(fg->v_cio, "[", 1);
                stream_write_data(fg->v_cio, title, -1);
                stream_write_data(fg->v_cio, package->p, -1);
                stream_write_data(fg->v_cio, "]\n", 2);
                while (*p != '\0') {
                    while (*p != '\0' && *p != '\t') {
                        p++;
                    }
                    stream_write_data(fg->v_cio, libver, p - libver);
                    stream_write_data(fg->v_cio, ": ", 2);
                    if (*p != '\0') {
                        p++;
                    }
                    libver = p;
                    while (*p != '\0' && *p != '\n') {
                        p++;
                    }
                    stream_write_data(fg->v_cio, libver, p - libver);
                    if (*p != '\0') {
                        p++;
                    }
                    libver = p;
                    stream_write_data(fg->v_cio, "\n", 1);
                }
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

    StrBuf_add_r(&path, fs->fox_home);
    StrBuf_add(&path, SEP_S "import" SEP_S, -1);
    show_lib_version_sub(&mem, &path, &package, "Native library: ");

    Mem_close(&mem);
}

static void show_configure_path(PtrList *lst, const char *title)
{
    int first = TRUE;

    stream_write_data(fg->v_cio, title, -1);
    for (; lst != NULL; lst = lst->next) {
        int exists = exists_file(lst->u.c) == EXISTS_DIR;
        if (!first) {
            first = FALSE;
            stream_write_data(fg->v_cio, ", ", 2);
        }
        stream_write_data(fg->v_cio, lst->u.c, -1);
        if (!exists) {
            stream_write_data(fg->v_cio, " *NOT EXISTS*", -1);
        }
    }
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

    stream_write_data(fg->v_cio, "[Configuration]\nFOX_ERROR : ", -1);
    p = Hash_get(&fs->envs, "FOX_ERROR", -1);
    if (defs[ENVSET_ERROR]) {
        stream_write_data(fg->v_cio, fv->err_dst, -1);
    } else {
        stream_write_data(fg->v_cio, "(stderr)", -1);
    }

    stream_write_data(fg->v_cio, "\nFOX_LANG: ", -1);
    p = Hash_get(&fs->envs, "FOX_LANG", -1);
    if (p != NULL) {
        stream_write_data(fg->v_cio, p, -1);
    } else {
        stream_write_data(fg->v_cio, "(", 1);
        stream_write_data(fg->v_cio, get_default_locale(), -1);
        stream_write_data(fg->v_cio, ")", 1);
    }

    stream_write_data(fg->v_cio, "\nFOX_MAX_ALLOC: ", -1);
    if (defs[ENVSET_MAX_ALLOC]) {
        sprintf(ctmp, "%d KB", fs->max_alloc / 1024);
    } else {
        sprintf(ctmp, "(%d KB)", fs->max_alloc / 1024);
    }
    stream_write_data(fg->v_cio, ctmp, -1);

    stream_write_data(fg->v_cio, "\nFOX_MAX_STACK: ", -1);
    if (defs[ENVSET_MAX_STACK]) {
        sprintf(ctmp, "%d", fs->max_stack);
    } else {
        sprintf(ctmp, "(%d)", fs->max_stack);
    }
    stream_write_data(fg->v_cio, ctmp, -1);

    show_configure_path(fs->import_path, "\nFOX_IMPORT: ");
    show_configure_path(fs->resource_path, "\nFOX_RESOURCE: ");

    stream_write_data(fg->v_cio, "\nFOX_TZ: ", -1);
    if (default_timezone) {
        if (tz != NULL) {
            if (tz_error) {
                stream_write_data(fg->v_cio, "*UNKNOWN*", -1);
            } else {
                stream_write_data(fg->v_cio, "(", 1);
                stream_write_data(fg->v_cio, tz->name, -1);
                stream_write_data(fg->v_cio, ")", 1);
            }
        } else {
            stream_write_data(fg->v_cio, "(Etc/UTC)", -1);
        }
    } else {
        stream_write_data(fg->v_cio, tz->name, -1);
    }
    stream_write_data(fg->v_cio, "\n", 1);
}
void print_foxinfo()
{
    show_version();
    show_lib_version();
    show_configure();
}
static void print_foxver(void)
{
    char cbuf[32];

#ifdef DEBUGGER
    stream_write_data(fg->v_cio, "foxd ", -1);
#else
    stream_write_data(fg->v_cio, "fox ", -1);
#endif

    sprintf(cbuf, "%d.%d.%d", FOX_VERSION_MAJOR, FOX_VERSION_MINOR, FOX_VERSION_REVISION);
    stream_write_data(fg->v_cio, cbuf, -1);
    sprintf(cbuf, ", %dbit", (int)sizeof(void*) * 8);
    stream_write_data(fg->v_cio, cbuf, -1);
#ifndef NO_DEBUG
    stream_write_data(fg->v_cio, " (debug-build)", -1);
#endif
    stream_write_data(fg->v_cio, "\n", 1);
}

static int parse_args(ArgumentInfo *ai, int argc, const char **argv)
{
    int i;
    char *srcfile = NULL;

    for (i = 1; argv[i] != NULL; i++) {
        const char *p = argv[i];
        if (p[0] == '-') {
            if (p[1] == '-') {
                // --
                int j;
                for (j = 2; p[j] != '\0'; j++) {
                    char ch = p[j];
                    if (!isalnumu_fox(ch)) {
                        throw_errorf(fs->mod_lang, "ArgumentError", "Unknown option %q", p);
                        return FALSE;
                    }
                }
                srcfile = str_printf("%r" SEP_S "script" SEP_S "%s.fox", fs->fox_home, p + 2);
            } else {
                switch (p[1]) {
                case 'D': {
                    const char *pkey = p + 2;
                    const char *pval = pkey;
                    while (*pval != '\0' && *pval != '=') {
                        pval++;
                    }
                    if (*pval == '=') {
                        RefStr *key = intern(pkey, pval - pkey);
                        char *val = str_dup_p(pval + 1, -1, &fg->st_mem);
                        Hash_add_p(&fs->envs, &fg->st_mem, key, val);
                    } else {
                        throw_errorf(fs->mod_lang, "ArgumentError", "Unknown option %q", p);
                        return FALSE;
                    }
                    break;
                }
#ifdef DEBUGGER
                case 'c':
                    ai->syntax_only = TRUE;
                    break;
#else
                case 'c':
                    throw_errorf(fs->mod_lang, "ArgumentError", "Use foxd -c for validate syntax", p);
                    return FALSE;
#endif
                case 'v':
#ifdef DEBUGGER
                    if (ai->syntax_only || argv[i + 1] != NULL) {
#else
                    if (argv[i + 1] != NULL) {
#endif
                        throw_errorf(fs->mod_lang, "ArgumentError", "Cannot use '-v' with other options");
                        return FALSE;
                    } else {
                        init_fox_stack();
                        print_foxver();
                    }
                    return FALSE;
                case 'e':
                    i++;
                    if (argv[i] != NULL) {
                        // fox -e "puts 'hello'"
                        ai->src_raw = argv[i];
                    } else {
                        throw_errorf(fs->mod_lang, "ArgumentError", "'-e' needs one or more argument");
                        return FALSE;
                    }
                    goto FINALLY;
                default:
                    throw_errorf(fs->mod_lang, "ArgumentError", "Unknown option %q", p);
                    return FALSE;
                }
            }
        } else {
            // ファイル名を取得
            if (is_absolute_path(p, -1)) {
                srcfile = str_dup_p(p, -1, NULL);
            } else {
                // 相対パス
                srcfile = str_printf("%r" SEP_S "%s", fv->cur_dir, p);
            }
            goto FINALLY;
        }
    }
FINALLY:
    fv->argc = argc - i - 1;
    fv->argv = argv + i + 1;
    if (srcfile != NULL) {
        make_path_regularize(srcfile, strlen(srcfile));
        ai->srcfile = srcfile;
    }

    return TRUE;
}

void print_last_error()
{
    if (fs->cgi_mode){
        fv->err_dst = "stdout";
    } else {
        fv->err_dst = "stderr";
    }
    if (fg->error != VALUE_NULL && strcmp(fv->err_dst, "null") != 0) {
        StrBuf sb;
        StrBuf_init(&sb, 0);

        if (strcmp(fv->err_dst, "stdout") == 0) {
            fox_error_dump(&sb, FALSE);
            if (fg->v_cio != VALUE_NULL) {
                if (fs->cgi_mode){
                    send_headers();
                }
                stream_write_data(fg->v_cio, sb.p, sb.size);
            }
        } else if (strcmp(fv->err_dst, "stderr") == 0) {
            if (fg->v_cio != VALUE_NULL) {
                stream_flush_sub(fg->v_cio);
            }
            fox_error_dump(&sb, FALSE);
#ifdef WIN32
            if (fv->console_error) {
                // UTF-16で出力
                win_write_console(STDERR_FILENO, sb.p, sb.size);
            } else {
                write_fox(STDERR_FILENO, sb.p, sb.size);
            }
#else
            write_fox(STDERR_FILENO, sb.p, sb.size);
#endif
        } else {
            FileHandle fh = open_errorlog_file();
            fox_error_dump(&sb, TRUE);
            close_fox(fh);
        }
        StrBuf_close(&sb);
    }
}

int main_fox(int argc, const char **argv)
{
    ArgumentInfo ai;
    RefNode *mod;
    RefStr *name_startup;
    int ret = FALSE;

    memset(&ai, 0, sizeof(ai));
    init_stdio();

    // デフォルト値
    fs->max_alloc = 32 * 1024 * 1024; // 32MB
    fs->max_stack = 32768;

    {
        // 実際のカレントディレクトリを取得
        // 末尾に/を付ける
        char *pwd = get_current_directory();
        int pwd_len = strlen(pwd);
        RefStr *rs = refstr_new_n(fs->cls_file, pwd_len + 1);
        memcpy(rs->c, pwd, pwd_len);
        if (rs->c[pwd_len - 1] != SEP_C) {
            rs->c[pwd_len] = SEP_C;
            rs->c[pwd_len + 1] = '\0';
        } else {
            rs->size = pwd_len;
            rs->c[pwd_len] = '\0';
        }
        fv->cur_dir = rs;
        free(pwd);
    }

    if (!parse_args(&ai, argc, argv)) {
        goto ERROR_END;
    }
    fox_init_compile(FALSE);
    load_htfox();

    if (ai.srcfile != NULL) {
        mod = get_module_by_file(ai.srcfile);
        if (mod != NULL) {
            mod->u.m.src_path = ai.srcfile;
        }
    } else if (ai.src_raw != NULL) {
        mod = get_module_from_src(ai.src_raw, -1);
        if (mod != NULL) {
            mod->u.m.src_path = "[argument]";
        }
    } else {
#ifdef DEBUGGER
        mod = get_module_from_src("", 0);
#else
        throw_errorf(fs->mod_lang, "ArgumentError", "Missing filename");
        goto ERROR_END;
#endif
    }
    // ソースを読み込んだ後で実行
    if (fs->cgi_mode) {
        cgi_init_responce();
    }
    init_fox_stack();

    if (mod == NULL) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(ai.srcfile, -1));
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
#ifdef DEBUGGER
    if (!ai.syntax_only) {
        if (ai.srcfile != NULL || ai.src_raw != NULL) {
            fox_debugger(mod);
        } else {
            fox_intaractive(mod);
        }
    }
#else
    if (!fox_run(mod)) {
        goto ERROR_END;
    }
#endif
    ret = TRUE;

ERROR_END:
    print_last_error();
    fox_close();
    return ret;
}

